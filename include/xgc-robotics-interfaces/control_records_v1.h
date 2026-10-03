#ifndef XGC_ROBOTICS_INTERFACES_CONTROL_RECORDS_V1_H
#define XGC_ROBOTICS_INTERFACES_CONTROL_RECORDS_V1_H

#include <stdint.h>

/* "xgc.position_target/1": local-frame position/velocity/acceleration target. */
typedef struct xgc_position_target_v1 {
  double stamp;
  double position[3];
  double velocity[3];
  double acceleration[3];
  double yaw;             /* rad; as PositionTarget.yaw */
  double yaw_rate;
  uint16_t type_mask;     /* PositionTarget IGNORE_* bits */
  uint8_t coordinate_frame;
  uint8_t reserved[5];
} xgc_position_target_v1;

/* "xgc.planar_pva/1": planar position-velocity-acceleration setpoint. */
typedef struct xgc_planar_pva_v1 {
  double stamp;           /* header.stamp */
  double x;
  double y;
  double yaw;
  double vx;
  double vy;
  double ax;
  double ay;
} xgc_planar_pva_v1;

/* "xgc.controller_status/1": the controller's control-region state name. */
typedef struct xgc_controller_status_v1 {
  double stamp;
  char state[48];         /* NUL-terminated */
} xgc_controller_status_v1;

#ifdef __cplusplus
static_assert(sizeof(xgc_position_target_v1) == 104, "xgc_position_target_v1");
static_assert(sizeof(xgc_planar_pva_v1) == 64, "xgc_planar_pva_v1");
static_assert(sizeof(xgc_controller_status_v1) == 56, "xgc_controller_status_v1");
#else
_Static_assert(sizeof(xgc_position_target_v1) == 104, "xgc_position_target_v1");
_Static_assert(sizeof(xgc_planar_pva_v1) == 64, "xgc_planar_pva_v1");
_Static_assert(sizeof(xgc_controller_status_v1) == 56, "xgc_controller_status_v1");
#endif

#endif
