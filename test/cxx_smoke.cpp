#include <cstddef>
#include <type_traits>
#include <xgc-robotics-interfaces/robotics_interfaces_v1.h>
static_assert(std::is_standard_layout<xgc_paired_state_v1>::value, "paired standard layout");
static_assert(std::is_trivially_copyable<xgc_paired_state_v1>::value, "paired byte transport");
static_assert(sizeof(xgc_paired_state_v1) == 96, "paired ABI size");
static_assert(offsetof(xgc_paired_state_v1, twist_stamp_sec) == 8, "independent stamp ABI");
static_assert(offsetof(xgc_paired_state_v1, linear_velocity) == 72, "velocity ABI offset");
int main() { xgc_position_target_v1 command{}; command.type_mask = 7; return command.type_mask != 7; }
