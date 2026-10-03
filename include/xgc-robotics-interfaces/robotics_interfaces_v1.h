#ifndef XGC_ROBOTICS_INTERFACES_V1_H
#define XGC_ROBOTICS_INTERFACES_V1_H

#include <stdint.h>
#include <xgc-robotics-interfaces/control_records_v1.h>
#include <xgc-robotics-interfaces/paired_state_v1.h>

/* Shared port payload records, copied byte for byte from their v1 schemas.
 * Source measurement stamps are separate from the transport envelope.
 * Clock/replay transport, estimator and reference policy payloads are owned
 * outside this component. Schema identifiers and record layouts are unchanged. */

/* "xgc.imu/1": body-frame specific force and angular rate. */
typedef struct xgc_imu_v1 {
  double stamp;
  double accel[3];  /* m/s^2, includes gravity reaction (+g on z at rest) */
  double gyro[3];   /* rad/s */
} xgc_imu_v1;

/* "xgc.attitude_target/1": commanded attitude and normalized thrust. */
typedef struct xgc_attitude_target_v1 {
  double stamp;
  double q_wxyz[4];
  double thrust;          /* normalized [0, 1] */
  uint32_t ignore_thrust; /* nonzero when the thrust field is not commanded */
  uint32_t reserved;
} xgc_attitude_target_v1;

/* "xgc.attitude_target/2": complete MAVROS AttitudeTarget payload.
 * /1 remains byte-for-byte unchanged for existing thrust observers.
 * type_mask preserves MAVROS IGNORE_* bits; ignored payload is not rewritten.
 * Quaternion rotates body FLU into ENU; body_rate is in FLU rad/s. */
typedef struct xgc_attitude_target_v2 {
  double stamp;
  double q_wxyz[4];
  double body_rate[3];
  double thrust;          /* normalized [0, 1], unless IGNORE_THRUST is set */
  uint32_t type_mask;
  uint32_t reserved;
} xgc_attitude_target_v2;

/* "xgc.pose/1": position and orientation in the Session world frame. */
typedef struct xgc_pose_v1 {
  double stamp;
  double position[3];
  double q_wxyz[4];
} xgc_pose_v1;

/* "xgc.fcu_state/1": mavros_msgs/State. */
typedef struct xgc_fcu_state_v1 {
  double stamp;
  uint8_t connected;
  uint8_t armed;
  uint8_t guided;
  uint8_t manual_input;
  uint8_t system_status;
  uint8_t reserved[3];
  char mode[32];          /* NUL-terminated, e.g. "OFFBOARD" */
} xgc_fcu_state_v1;

/* "xgc.twist/1": geometry_msgs/TwistStamped. */
typedef struct xgc_twist_v1 {
  double stamp;
  double linear[3];
  double angular[3];
} xgc_twist_v1;

/* "xgc.battery/1": the sensor_msgs/BatteryState fields the controller reads. */
typedef struct xgc_battery_v1 {
  double stamp;
  double voltage;
  double percentage;      /* 0..1 */
} xgc_battery_v1;

/* "xgc.command/1": an operator command string (std_msgs/String on /command). */
typedef struct xgc_command_v1 {
  char text[64];          /* NUL-terminated */
} xgc_command_v1;

/* "xgc.body_rate_thrust/1": body rates + normalized thrust
 * (mavros_msgs/AttitudeTarget with the attitude ignored). */
typedef struct xgc_body_rate_thrust_v1 {
  double stamp;
  double body_rate[3];
  double thrust;
} xgc_body_rate_thrust_v1;

/* "xgc.fcu_request/1": a MAVROS service request the edge must make. */
typedef struct xgc_fcu_request_v1 {
  double stamp;
  uint32_t kind;          /* 1 arm/disarm (cmd/command 400), 2 set_mode */
  uint32_t arm;           /* kind 1: 1 arm, 0 disarm */
  char mode[32];          /* kind 2: custom mode, NUL-terminated */
} xgc_fcu_request_v1;

#ifdef __cplusplus
static_assert(sizeof(xgc_imu_v1) == 56, "xgc_imu_v1");
static_assert(sizeof(xgc_attitude_target_v1) == 56, "xgc_attitude_target_v1");
static_assert(sizeof(xgc_attitude_target_v2) == 80, "xgc_attitude_target_v2");
static_assert(sizeof(xgc_pose_v1) == 64, "xgc_pose_v1");
static_assert(sizeof(xgc_fcu_state_v1) == 48, "xgc_fcu_state_v1");
static_assert(sizeof(xgc_twist_v1) == 56, "xgc_twist_v1");
static_assert(sizeof(xgc_battery_v1) == 24, "xgc_battery_v1");
static_assert(sizeof(xgc_command_v1) == 64, "xgc_command_v1");
static_assert(sizeof(xgc_body_rate_thrust_v1) == 40, "xgc_body_rate_thrust_v1");
static_assert(sizeof(xgc_fcu_request_v1) == 48, "xgc_fcu_request_v1");
#else
_Static_assert(sizeof(xgc_imu_v1) == 56, "xgc_imu_v1");
_Static_assert(sizeof(xgc_attitude_target_v1) == 56, "xgc_attitude_target_v1");
_Static_assert(sizeof(xgc_attitude_target_v2) == 80, "xgc_attitude_target_v2");
_Static_assert(sizeof(xgc_pose_v1) == 64, "xgc_pose_v1");
_Static_assert(sizeof(xgc_fcu_state_v1) == 48, "xgc_fcu_state_v1");
_Static_assert(sizeof(xgc_twist_v1) == 56, "xgc_twist_v1");
_Static_assert(sizeof(xgc_battery_v1) == 24, "xgc_battery_v1");
_Static_assert(sizeof(xgc_command_v1) == 64, "xgc_command_v1");
_Static_assert(sizeof(xgc_body_rate_thrust_v1) == 40, "xgc_body_rate_thrust_v1");
_Static_assert(sizeof(xgc_fcu_request_v1) == 48, "xgc_fcu_request_v1");
#endif

#endif
