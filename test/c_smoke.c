#include <stddef.h>
#include <string.h>
#include <xgc-robotics-interfaces/robotics_interfaces_v1.h>
_Static_assert(sizeof(xgc_paired_state_v1) == 96, "paired ABI size");
_Static_assert(offsetof(xgc_paired_state_v1, position) == 16, "position ABI offset");
_Static_assert(offsetof(xgc_paired_state_v1, orientation_xyzw) == 40, "quaternion ABI offset");
_Static_assert(offsetof(xgc_paired_state_v1, linear_velocity) == 72, "velocity ABI offset");
int main(void) {
  xgc_paired_state_v1 original = {0}, received;
  original.pose_stamp_sec = 1.25;
  original.twist_stamp_sec = 1.5;
  original.orientation_xyzw[3] = 1.0;
  memcpy(&received, &original, sizeof(received));
  return memcmp(&received, &original, sizeof(received)) != 0;
}
