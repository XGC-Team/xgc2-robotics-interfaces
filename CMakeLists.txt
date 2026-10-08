cmake_minimum_required(VERSION 3.16)
project(XgcRoboticsInterfaces VERSION 0.2.0 LANGUAGES NONE)

include(CMakePackageConfigHelpers)
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Header installation directory")
set(CMAKE_INSTALL_DATADIR "share" CACHE PATH "Architecture-independent data directory")
set(interfaces_package_dir "${CMAKE_INSTALL_DATADIR}/cmake/XgcRoboticsInterfaces")

add_library(xgc_robotics_interfaces INTERFACE)
add_library(XgcRoboticsInterfaces::Interfaces ALIAS xgc_robotics_interfaces)
set_target_properties(xgc_robotics_interfaces PROPERTIES EXPORT_NAME Interfaces)
target_include_directories(xgc_robotics_interfaces INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(xgc_chassis_hold INTERFACE)
add_library(XgcRoboticsInterfaces::ChassisHold ALIAS xgc_chassis_hold)
set_target_properties(xgc_chassis_hold PROPERTIES EXPORT_NAME ChassisHold)
target_compile_features(xgc_chassis_hold INTERFACE cxx_std_20)
target_include_directories(xgc_chassis_hold INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(xgc_chassis_hold INTERFACE XgcXrpc::http PkgConfig::XGC_HOLD_JSONCPP)

install(FILES
  include/xgc-robotics-interfaces/paired_state_v1.h
  include/xgc-robotics-interfaces/control_records_v1.h
  include/xgc-robotics-interfaces/robotics_interfaces_v1.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/xgc-robotics-interfaces")
install(DIRECTORY include/xgc2/chassis_hold
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/xgc2")
install(DIRECTORY contracts/
  DESTINATION "${CMAKE_INSTALL_DATADIR}/xgc2-robotics-interfaces/contracts")
install(TARGETS xgc_robotics_interfaces xgc_chassis_hold EXPORT XgcRoboticsInterfacesTargets)
install(EXPORT XgcRoboticsInterfacesTargets
  NAMESPACE XgcRoboticsInterfaces::
  DESTINATION "${interfaces_package_dir}")
configure_package_config_file(cmake/XgcRoboticsInterfacesConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/XgcRoboticsInterfacesConfig.cmake"
  INSTALL_DESTINATION "${interfaces_package_dir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/XgcRoboticsInterfacesConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/XgcRoboticsInterfacesConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/XgcRoboticsInterfacesConfigVersion.cmake"
  DESTINATION "${interfaces_package_dir}")
