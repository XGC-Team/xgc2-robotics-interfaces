# XGC2 Robotics Interfaces

Header-only robotics payloads, the simulation-v1 contract, and an optional
native chassis HOLD provider.
Source owner: [XGC-Team/xgc2-robotics-interfaces](https://github.com/XGC-Team/xgc2-robotics-interfaces).
The product exports 14 existing records: one paired pose/velocity measurement,
three control records, and ten measurement/control records. C type names, schema
identifiers, layouts and measurement stamps remain unchanged. This component
adds no conversion, freshness, frame, estimator or control policy.

The five simulation FCU/provider request, result and batch-state records belong
to the lightweight simulator product and are excluded from this package.
The base Interfaces target has no SDK or ROS dependency. ChassisHold explicitly
requires the XgcXrpc SDK and JsonCpp >=1.9.4. There is no runtime executable or
algorithm plugin in this package.
The migration audit in `.xgc2/migration.json` records the original component
hashes; public include guards, filenames and CMake names were renamed once
before the first release. No old-name compatibility aliases exist.

```cmake
find_package(XgcRoboticsInterfaces 0.2.0 CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE XgcRoboticsInterfaces::Interfaces)
```

Public headers are `xgc-robotics-interfaces/robotics_interfaces_v1.h`,
`xgc-robotics-interfaces/control_records_v1.h` and
`xgc-robotics-interfaces/paired_state_v1.h`. Consumers use C11 or C++14.
The paired record is 96 bytes with independent pose/twist measurement stamps;
its position, quaternion and linear-velocity offsets are 16, 40 and 72 bytes.

Package version `0.2.0-1~focal` installs headers, relocatable CMake exports,
contracts and the license. The base payload layouts remain unchanged. The
optional component is selected explicitly:

```cmake
find_package(XgcRoboticsInterfaces 0.2.0 CONFIG REQUIRED COMPONENTS ChassisHold)
target_link_libraries(my_native_host PRIVATE XgcRoboticsInterfaces::ChassisHold)
```

ChassisHold requires C++20 and an SDK-supported compiler/runtime; the existing
Focal C11/C++14 payload CI does not prove this new component's deployment ABI.
Installed ChassisHold has been exercised by actual native Gazebo multi-robot
HOLD/zero/release tests using the installed SDK. Its standalone host or embedded
adapter has a fixed queue and roster; the embedding app owns the executor and
producer gate. See `contracts/chassis-hold-v1.md` and `simulation-v1.md`.

Push and PR CI on main reuse the Runtime SDK pattern: build one all Deb, refuse
artifact overwrite, then download those identical bytes on native amd64 and
arm64 runners. Both views install the actual Deb and compile/run installed and
relocated C11/C++14 consumers. Negative controls require rejection after each
header is removed, the package config is absent, the requested version is wrong,
and the package is actually uninstalled. Each view emits a strict
`xgc2.build-artifact.v1` manifest. Only Debs and source-bound manifests are
retained for 14 days.

CI uses the controlled Focal dev image `1.0.0`, multiarch digest
`sha256:a1fe144cca63b8b90b76bf152147ab753075399a82fbd339798023c667e4a64a`:
amd64 `fd7e392b5d7691171b47b4161345fc87e09f2ef050ab64e90cee8aef48ee1106`,
arm64 `9ae3abd759991e7918a835bcb8d3a80aefb200a82239223b4bab7b6c457c79e5`.
It does not install ROS or XGC2 dependencies. Source is read-only, writable host
binds use the calling UID/GID, and containers have no external network.

```sh
.xgc2/scripts/build_debs_in_docker.sh --output-dir "$PWD/debs" --work-dir /tmp/robotics-interfaces-build
```

The central xgc2-devops orchestrator alone publishes APT indexes. Ordinary
releases reuse exact-source push CI artifacts; `release.yml` provides only the
standard central prepare/compatibility contract. Product CI does not prove
production APT visibility or downstream installation/experiment acceptance.
