#ifndef XGC_ROBOTICS_INTERFACES_PAIRED_STATE_V1_H
#define XGC_ROBOTICS_INTERFACES_PAIRED_STATE_V1_H

#include <stddef.h>

/* A pose and linear velocity paired by their original measurement stamps.
 * This record imposes no freshness, frame, control, or algorithm policy. */
typedef struct xgc_paired_state_v1 {
  double pose_stamp_sec;
  double twist_stamp_sec;
  double position[3];
  double orientation_xyzw[4];
  double linear_velocity[3];
} xgc_paired_state_v1;

#ifdef __cplusplus
static_assert(sizeof(xgc_paired_state_v1) == 96, "xgc_paired_state_v1 size");
static_assert(offsetof(xgc_paired_state_v1, pose_stamp_sec) == 0, "pose stamp offset");
static_assert(offsetof(xgc_paired_state_v1, twist_stamp_sec) == 8, "twist stamp offset");
static_assert(offsetof(xgc_paired_state_v1, position) == 16, "position offset");
static_assert(offsetof(xgc_paired_state_v1, orientation_xyzw) == 40, "orientation offset");
static_assert(offsetof(xgc_paired_state_v1, linear_velocity) == 72, "velocity offset");
#else
_Static_assert(sizeof(xgc_paired_state_v1) == 96, "xgc_paired_state_v1 size");
_Static_assert(offsetof(xgc_paired_state_v1, pose_stamp_sec) == 0, "pose stamp offset");
_Static_assert(offsetof(xgc_paired_state_v1, twist_stamp_sec) == 8, "twist stamp offset");
_Static_assert(offsetof(xgc_paired_state_v1, position) == 16, "position offset");
_Static_assert(offsetof(xgc_paired_state_v1, orientation_xyzw) == 40, "orientation offset");
_Static_assert(offsetof(xgc_paired_state_v1, linear_velocity) == 72, "velocity offset");
#endif

#endif
