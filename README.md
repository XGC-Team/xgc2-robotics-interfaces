# XGC2 Robotics Interfaces

Fourteen C11/C++14 robotics payload records and the simulation-v1 contract.
Payload type names, schema identifiers and layouts remain unchanged.
The records comprise one paired pose/velocity measurement, three control
records and ten measurement/control records. The 96-byte paired record has
independent pose/twist stamps; position, quaternion and linear velocity start
at byte offsets 16, 40 and 72. The library adds no conversion, freshness,
frame, estimator or control policy. No runtime, ROS or RPC dependency.

```cmake
find_package(XgcRoboticsInterfaces 0.2.0 CONFIG REQUIRED COMPONENTS Interfaces)
target_link_libraries(my_consumer PRIVATE XgcRoboticsInterfaces::Interfaces)
```

Public headers are `xgc-robotics-interfaces/robotics_interfaces_v1.h`,
`control_records_v1.h` and `paired_state_v1.h`.

Chassis HOLD is provided by the separate
[xgc2-chassis-hold](https://github.com/XGC-Team/xgc2-chassis-hold) library.

```sh
.xgc2/scripts/build_debs_in_docker.sh --output-dir "$PWD/debs" --work-dir /tmp/robotics-interfaces-build
```
