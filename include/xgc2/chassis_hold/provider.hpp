#pragma once

#include "state.hpp"
#include <xgc2/xrpc/http.hpp>
#include <xgc2/xrpc/runtime_policy.hpp>
#include <json/json.h>
#include <algorithm>
#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace xgc2::chassis_hold {

struct Options {
  std::string socket_path;
  std::string instance_id;
  std::string target_id;
  std::vector<std::string> robot_ids;
  std::vector<std::pair<std::string, std::string>> environment;
  bool embedded = false;
  // Embedded routes share their framework owner's actually enforced policy;
  // they neither resolve the environment again nor invent another listener.
  std::shared_ptr<const xrpc::RuntimePolicy> host_policy;
};

// Called once by the process composition root with its explicit environment.
// Only reserved XRPC settings are retained; unrelated credentials are excluded.
inline std::vector<std::pair<std::string, std::string>>
environment_snapshot(char *const *environment) {
  std::vector<std::pair<std::string, std::string>> snapshot;
  if (environment)
    for (auto entry = environment; *entry; ++entry) {
      const std::string value(*entry);
      if (value.rfind("XGC2_XRPC_", 0) != 0) continue;
      const auto equals = value.find('=');
      snapshot.emplace_back(value.substr(0, equals),
          equals == std::string::npos ? "" : value.substr(equals + 1));
    }
  return snapshot;
}

// One process-wide HTTP owner; one caller-owned native command executor.
// Construct before the native loop and destroy after it has stopped. Slots
// retain requests/replies until native work terminates even if callers leave.
class Provider {
public:
  explicit Provider(Options options)
      : options_(std::move(options)), state_(options_.robot_ids.size()),
        policy_(options_.embedded ? options_.host_policy :
          std::make_shared<xrpc::RuntimePolicy>(resolve_policy(options_.environment))),
        limits_(policy_ ? xrpc::http_limits(*policy_) : xrpc::HttpLimits{}) {
    if (options_.embedded && (!policy_ || !options_.environment.empty()))
      throw std::invalid_argument("embedded hold requires shared host policy and no environment copy");
    options_.environment.clear();
    validate_options();
    if (options_.embedded) {
      healthy_.store(true, std::memory_order_release);
      return;
    }
    std::promise<void> ready;
    auto started = ready.get_future();
    io_ = std::thread([this, ready = std::move(ready)]() mutable {
      bool announced = false;
      try {
        xrpc::UnixOptions endpoint;
        endpoint.path = options_.socket_path;
        xrpc::HttpIdentity identity;
        identity.instance_id = options_.instance_id;
        identity.discovery_targets = {"/v1/chassis/descriptor"};
        xrpc::HttpServer server(endpoint,
            [this](xrpc::HttpRequest request, xrpc::HttpReply reply) {
              handle(std::move(request), std::move(reply));
            }, limits_, std::move(identity));
        server.set_wakeup_handler([this] { finish(); });
        healthy_.store(true, std::memory_order_release);
        ready.set_value();
        announced = true;
        while (!stopping_.load(std::memory_order_acquire)) {
          server.poll(std::chrono::milliseconds(5));
          finish();
        }
        server.request_stop();
        // Native callers are already quiescent. Release transport replies on
        // the IO owner before releasing the endpoint lease.
        for (auto &slot : slots_)
          slot.reply = {};
      } catch (...) {
        if (!announced)
          ready.set_exception(std::current_exception());
      }
      healthy_.store(false, std::memory_order_release);
    });
    try {
      started.get();
    } catch (...) {
      io_.join();
      throw;
    }
  }
  ~Provider() {
    stopping_.store(true, std::memory_order_release);
    if (io_.joinable())
      io_.join();
    if (options_.embedded)
      for (auto &slot : slots_) slot.reply = {};
  }
  Provider(const Provider &) = delete;
  Provider &operator=(const Provider &) = delete;

  // Native thread only. The zero sink applies to its sole command output and
  // reports success only after the native write/command-state update returns.
  // No JSON, socket access, waiting or transport-owned reply is used here.
  std::size_t control_tick(State::ZeroFn zero, void *context,
                           std::size_t budget = max_robots) noexcept {
    std::size_t processed = 0;
    auto tail = tail_.load(std::memory_order_relaxed);
    while (processed < budget && tail != head_.load(std::memory_order_acquire)) {
      auto &slot = slots_[queue_[tail % queue_capacity]];
      slot.phase.store(Phase::Executing, std::memory_order_release);
      if (slot.cancelled.load(std::memory_order_acquire))
        slot.result = state_.snapshot(Outcome::Cancelled);
      else if (xrpc::Clock::now() >= slot.deadline)
        slot.result = state_.snapshot(Outcome::Expired);
      else
        slot.result = state_.apply(slot.command, zero, context);
      slot.phase.store(Phase::Done, std::memory_order_release);
      ++tail;
      tail_.store(tail, std::memory_order_release);
      ++processed;
    }
    return processed;
  }
  bool held(std::size_t robot = 0) const noexcept { return state_.held(robot); }
  bool healthy() const noexcept { return healthy_.load(std::memory_order_acquire); }
  std::size_t pending() const noexcept {
    return pending_.load(std::memory_order_acquire);
  }
  const std::string &instance_id() const noexcept { return options_.instance_id; }

private:
  enum class Phase { Free, Queued, Executing, Done };
  struct Slot {
    std::atomic<Phase> phase{Phase::Free};
    std::atomic<bool> cancelled{false};
    Command command;
    xrpc::Clock::time_point deadline;
    Result result;
    xrpc::HttpReply reply; // IO thread exclusively.
  };
  static bool valid_id(const std::string &value) {
    if (value.empty() || value.size() > 128)
      return false;
    for (const unsigned char c : value)
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
        return false;
    return true;
  }
  void validate_options() {
    if (options_.socket_path.empty() || options_.socket_path.front() != '/' ||
        options_.instance_id.empty() || !valid_id(options_.target_id))
      throw std::invalid_argument("explicit chassis endpoint/instance/target required");
    for (std::size_t i = 0; i < options_.robot_ids.size(); ++i) {
      if (!valid_id(options_.robot_ids[i]))
        throw std::invalid_argument("invalid chassis robot id");
      for (std::size_t j = 0; j < i; ++j)
        if (options_.robot_ids[i] == options_.robot_ids[j])
          throw std::invalid_argument("duplicate chassis robot id");
    }
    const auto &l = limits_;
    if (!options_.embedded && (l.connections > queue_capacity || l.inflight > queue_capacity ||
        l.header_bytes > 4096 || l.request_bytes > 4096 || l.response_bytes > 8192 ||
        l.request_timeout > std::chrono::milliseconds(250)))
      throw std::invalid_argument("chassis HTTP resource ceiling exceeded");
  }
  static xrpc::RuntimePolicy resolve_policy(
      const std::vector<std::pair<std::string, std::string>> &environment) {
    xrpc::RuntimePolicyOptions policy;
    policy.environment = environment;
    policy.default_source = "chassis.hold.v1";
    policy.defaults = {{"HOST_MAX_CONNECTIONS", "32"}, {"HOST_MAX_IN_FLIGHT", "32"},
      {"MAX_HEADER_BYTES", "4096"}, {"MAX_REQUEST_BYTES", "4096"},
      {"MAX_RESPONSE_BYTES", "8192"}, {"CALL_TIMEOUT_MS", "250"},
      {"HEADER_TIMEOUT_MS", "250"}, {"IDLE_TIMEOUT_MS", "5000"},
      {"SHUTDOWN_TIMEOUT_MS", "1000"}};
    policy.ceilings = {{"HOST_MAX_CONNECTIONS", 32}, {"HOST_MAX_IN_FLIGHT", 32},
      {"MAX_HEADER_BYTES", 4096}, {"MAX_REQUEST_BYTES", 4096},
      {"MAX_RESPONSE_BYTES", 8192}, {"CALL_TIMEOUT_MS", 250}};
    return xrpc::resolve_runtime_policy(policy);
  }
  Json::Value effective_policy() const {
    Json::Value value;
    value["revision"] = Json::UInt64(policy_->revision());
    value["fields"] = Json::Value(Json::arrayValue);
    for (const auto &field : policy_->fields()) {
      Json::Value record;
      record["name"] = std::string(field.name);
      record["source"] = std::string(field.source);
      record["source_detail"] = field.source_detail;
      record["dynamic"] = false; // This provider exposes no live policy mutation.
      if (const auto *integer = std::get_if<std::int64_t>(&field.value))
        record["value"] = Json::Int64(*integer);
      else record["value"] = std::get<std::string>(field.value);
      if (field.ceiling) record["ceiling"] = Json::Int64(*field.ceiling);
      value["fields"].append(std::move(record));
    }
    return value;
  }
  static xrpc::HttpResponse json_response(Json::Value body, int status = 200) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    xrpc::HttpResponse response;
    response.status = status;
    response.headers.emplace_back("Content-Type", "application/json");
    response.body = Json::writeString(writer, body);
    return response;
  }
  Json::Value descriptor() const {
    Json::Value value;
    value["target_id"] = options_.target_id;
    value["service"] = "chassis.hold";
    value["api_version"] = "1";
    value["instance_id"] = options_.instance_id;
    value["profile"] = "http.v1";
    value["endpoint"]["kind"] = "unix";
    value["endpoint"]["address"] = options_.socket_path;
    value["max_batch"] = static_cast<Json::UInt>(max_robots);
    value["queue_capacity"] = static_cast<Json::UInt>(queue_capacity);
    value["completion"] = "command-gated";
    value["physical_stop_supported"] = false;
    value["embedded"] = options_.embedded;
    value["robots"] = Json::Value(Json::arrayValue);
    for (const auto &id : options_.robot_ids)
      value["robots"].append(id);
    return value;
  }
  bool parse(const std::string &body, Command &command) const {
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    builder["allowComments"] = false;
    builder["allowTrailingCommas"] = false;
    builder["strictRoot"] = true;
    builder["allowSingleQuotes"] = false;
    builder["failIfExtra"] = true;
    builder["rejectDupKeys"] = true;
    builder["allowSpecialFloats"] = false;
    builder["stackLimit"] = 8;
    Json::Value value;
    std::string error;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(body.data(), body.data() + body.size(), &value, &error) ||
        !value.isObject() || value.size() != 2 ||
        !value["expected_revision"].isUInt64() ||
        (value["expected_revision"].type() != Json::intValue &&
         value["expected_revision"].type() != Json::uintValue) ||
        !value["changes"].isArray())
      return false;
    const auto &changes = value["changes"];
    if (changes.empty() || changes.size() > max_robots)
      return false;
    command.mutate = true;
    command.expected_revision = value["expected_revision"].asUInt64();
    command.count = static_cast<std::uint8_t>(changes.size());
    std::array<bool, max_robots> seen{};
    for (Json::ArrayIndex n = 0; n < changes.size(); ++n) {
      const auto &change = changes[n];
      if (!change.isObject() || change.size() != 2 ||
          !change["robot_id"].isString() || !change["held"].isBool())
        return false;
      const auto id = change["robot_id"].asString();
      std::size_t index = 0;
      while (index < options_.robot_ids.size() && options_.robot_ids[index] != id)
        ++index;
      if (index == options_.robot_ids.size() || seen[index])
        return false;
      seen[index] = true;
      command.changes[n] = {static_cast<std::uint8_t>(index), change["held"].asBool()};
    }
    return true;
  }
public:
  // Framework IO owner only in embedded mode; its existing HttpServer performs
  // instance fencing, admission and native request deadlines before dispatch.
  void handle(xrpc::HttpRequest request, xrpc::HttpReply reply) {
    try {
      if (stopping_.load(std::memory_order_acquire)) {
        reply.complete(xrpc::http_error(503, "unavailable", "chassis hold stopping"));
        return;
      }
      if (request.target == "/v1/chassis/descriptor" && request.method == "GET" &&
          request.body.empty()) {
        reply.complete(json_response(descriptor()));
        return;
      }
      if (request.target == "/v1/chassis/runtime-policy" && request.method == "GET" &&
          request.body.empty()) {
        reply.complete(json_response(effective_policy()));
        return;
      }
      if (request.target != "/v1/chassis/hold") {
        reply.complete(xrpc::http_error(404, "not_found", "unknown chassis method"));
        return;
      }
      Command command;
      if (request.method == "POST") {
        if (request.body.size() > 4096) {
          reply.complete(xrpc::http_error(413, "resource_exhausted", "hold batch exceeds 4096 bytes"));
          return;
        }
        if (!parse(request.body, command)) {
          reply.complete(xrpc::http_error(400, "invalid_argument", "invalid hold batch"));
          return;
        }
      } else if (request.method != "GET" || !request.body.empty()) {
        reply.complete(xrpc::http_error(400, "invalid_argument", "GET or POST required"));
        return;
      }
      const auto head = head_.load(std::memory_order_relaxed);
      const auto tail = tail_.load(std::memory_order_acquire);
      std::size_t index = 0;
      while (index < queue_capacity &&
             slots_[index].phase.load(std::memory_order_acquire) != Phase::Free)
        ++index;
      if (index == queue_capacity || head - tail >= queue_capacity) {
        reply.complete(xrpc::http_error(429, "resource_exhausted", "hold queue full"));
        return;
      }
      auto &slot = slots_[index];
      slot.command = command;
      slot.deadline = std::min(request.deadline, xrpc::Clock::now()+std::chrono::milliseconds(250));
      slot.reply = std::move(reply);
      slot.cancelled.store(false, std::memory_order_relaxed);
      slot.phase.store(Phase::Queued, std::memory_order_release);
      queue_[head % queue_capacity] = static_cast<std::uint8_t>(index);
      pending_.fetch_add(1, std::memory_order_release);
      head_.store(head + 1, std::memory_order_release);
    } catch (const std::exception &) {
      reply.complete(xrpc::http_error(400, "invalid_argument", "invalid chassis request"));
    }
  }
  void finish() {
    for (auto &slot : slots_) {
      const auto phase = slot.phase.load(std::memory_order_acquire);
      if (phase == Phase::Queued) {
        slot.cancelled.store(slot.reply.cancelled(), std::memory_order_release);
        continue;
      }
      if (phase != Phase::Done)
        continue;
      const auto &result = slot.result;
      int status = 200;
      const char *stage = "observed", *code = "";
      switch (result.outcome) {
      case Outcome::Applied: stage = "applied"; break;
      case Outcome::Conflict: stage = "rejected"; status = 409; code = "conflict"; break;
      case Outcome::Invalid: stage = "rejected"; status = 400; code = "invalid_argument"; break;
      case Outcome::Expired: stage = "not-applied"; status = 504; code = "deadline_exceeded"; break;
      case Outcome::Cancelled: stage = "not-applied"; status = 499; code = "cancelled"; break;
      case Outcome::Failed: stage = "failed"; status = 503; code = "native_write_failed"; break;
      case Outcome::Snapshot: break;
      }
      Json::Value body;
      body["stage"] = stage;
      body["revision"] = Json::UInt64(result.revision);
      body["completion"] = "command-gated";
      body["physical_stop_confirmed"] = false;
      if (*code)
        body["error"]["code"] = code;
      body["robots"] = Json::Value(Json::arrayValue);
      for (std::size_t i = 0; i < options_.robot_ids.size(); ++i) {
        Json::Value robot;
        robot["robot_id"] = options_.robot_ids[i];
        robot["held"] = result.held[i];
        body["robots"].append(std::move(robot));
      }
      body["changes"] = Json::Value(Json::arrayValue);
      for (std::size_t i = 0; i < slot.command.count; ++i) {
        const auto change = slot.command.changes[i];
        Json::Value record;
        record["robot_id"] = options_.robot_ids[change.robot];
        record["requested_held"] = change.held;
        record["zero_applied"] = result.zero_applied[change.robot];
        body["changes"].append(std::move(record));
      }
      slot.reply.complete(json_response(std::move(body), status));
      slot.reply = {};
      slot.phase.store(Phase::Free, std::memory_order_release);
      pending_.fetch_sub(1, std::memory_order_release);
    }
  }
private:
  Options options_;
  State state_;
  const std::shared_ptr<const xrpc::RuntimePolicy> policy_;
  const xrpc::HttpLimits limits_;
  std::array<Slot, queue_capacity> slots_{};
  std::array<std::uint8_t, queue_capacity> queue_{};
  std::atomic<std::uint64_t> head_{0}, tail_{0};
  std::atomic<std::size_t> pending_{0};
  std::atomic<bool> stopping_{false}, healthy_{false};
  std::thread io_;
};
} // namespace xgc2::chassis_hold
