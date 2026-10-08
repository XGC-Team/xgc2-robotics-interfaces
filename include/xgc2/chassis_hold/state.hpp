#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace xgc2::chassis_hold {

inline constexpr std::size_t max_robots = 16;
inline constexpr std::size_t queue_capacity = 32;

struct Change {
  std::uint8_t robot = 0;
  bool held = false;
};
struct Command {
  bool mutate = false;
  std::uint64_t expected_revision = 0;
  std::uint8_t count = 0;
  std::array<Change, max_robots> changes{};
};
enum class Outcome { Snapshot, Applied, Conflict, Invalid, Expired, Cancelled, Failed };
struct Result {
  Outcome outcome = Outcome::Snapshot;
  std::uint64_t revision = 0;
  std::array<bool, max_robots> held{};
  std::array<bool, max_robots> zero_applied{};
};

// Owned exclusively by the native command executor. No transport, locks,
// allocation, clock access, callback lifetime registry or worker lives here.
class State {
public:
  using ZeroFn = bool (*)(void *, std::size_t) noexcept;
  explicit State(std::size_t robots) : robots_(robots) {
    if (robots == 0 || robots > max_robots)
      throw std::invalid_argument("chassis hold requires 1..16 robots");
  }
  bool held(std::size_t robot) const noexcept {
    return robot >= robots_ || held_[robot];
  }
  Result snapshot(Outcome outcome = Outcome::Snapshot) const noexcept {
    Result result;
    result.outcome = outcome;
    result.revision = revision_;
    result.held = held_;
    return result;
  }
  Result apply(const Command &command, ZeroFn zero, void *context) noexcept {
    if (!command.mutate)
      return snapshot();
    if (command.count == 0 || command.count > robots_ || zero == nullptr)
      return snapshot(Outcome::Invalid);
    std::array<bool, max_robots> seen{};
    for (std::size_t i = 0; i < command.count; ++i) {
      const auto index = command.changes[i].robot;
      if (index >= robots_ || seen[index])
        return snapshot(Outcome::Invalid);
      seen[index] = true;
    }
    if (command.expected_revision != revision_ ||
        revision_ == std::numeric_limits<std::uint64_t>::max())
      return snapshot(Outcome::Conflict);

    // Mask the complete batch before touching an actuator. Releases also
    // apply zero before opening, so an old cached command is never replayed.
    for (std::size_t i = 0; i < command.count; ++i)
      held_[command.changes[i].robot] = true;
    Result result;
    bool success = true;
    for (std::size_t i = 0; i < command.count; ++i) {
      const auto index = command.changes[i].robot;
      result.zero_applied[index] = zero(context, index);
      success = result.zero_applied[index] && success;
    }
    if (success)
      for (std::size_t i = 0; i < command.count; ++i)
        held_[command.changes[i].robot] = command.changes[i].held;
    // Hardware writes cannot be rolled back. On failure all affected gates
    // remain held and a new revision reports the visible partial effect.
    ++revision_;
    result.outcome = success ? Outcome::Applied : Outcome::Failed;
    result.revision = revision_;
    result.held = held_;
    return result;
  }
private:
  const std::size_t robots_;
  std::uint64_t revision_ = 0;
  std::array<bool, max_robots> held_{};
};
} // namespace xgc2::chassis_hold
