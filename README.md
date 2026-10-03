# XGC2 Robotics Interfaces

Header-only C payload interfaces for generic robotics measurements and controls.
Source owner: [XGC-Team/xgc2-robotics-interfaces](https://github.com/XGC-Team/xgc2-robotics-interfaces).
The product exports 14 existing records: one paired pose/velocity measurement,
three control records, and ten measurement/control records. C type names, schema
identifiers, layouts and measurement stamps remain unchanged. This component
adds no conversion, freshness, frame, estimator or control policy.

The five simulation FCU/provider request, result and batch-state records belong
to the lightweight simulator product and are excluded from this package.
There is no Runtime SDK or ROS dependency, runtime binary or algorithm plugin.
The migration audit in `.xgc2/migration.json` records the original component
hashes; public include guards, filenames and CMake names were renamed once
before the first release. No old-name compatibility aliases exist.

```cmake
find_package(XgcRoboticsInterfaces 0.1.0 CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE XgcRoboticsInterfaces::Interfaces)
```

Public headers are `xgc-robotics-interfaces/robotics_interfaces_v1.h`,
`xgc-robotics-interfaces/control_records_v1.h` and
`xgc-robotics-interfaces/paired_state_v1.h`. Consumers use C11 or C++14.
The paired record is 96 bytes with independent pose/twist measurement stamps;
its position, quaternion and linear-velocity offsets are 16, 40 and 72 bytes.

First package: `libxgc2-robotics-interfaces-dev 0.1.0-1~focal`, Architecture all,
with no hard Depends. It installs only headers, the relocatable CMake INTERFACE
export and license; it has no ROS or SDK dependency and owns no simulation
lifecycle schema. Both consumers declare the header owner as a build dependency,
so their runtime package relationships do not introduce a release cycle.

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
