# OmniHand 3 Ultra M (H3UM T1.3/T1.5) Python API

## Overview

Python exposes one `OmniHand3UltraM` class for both revisions. T1.3 has 20 active joints and default device ID `9`; T1.5 has 21 active joints and default device ID `1`.

```python
from omnihand import (
    ControlMode,
    H3UMRevision,
    HandType,
    OmniHand3UltraM,
    OmniHand3UltraMSolver,
)
```

## Factories

```python
# T1.3: create_hand_by_*, 20 joints, default ID 9
t1p3 = OmniHand3UltraM.create_hand_by_zlgcan(
    hand_type=HandType.LEFT,
    canfd_device_id=0,
    canfd_channel_id=0,
    tactile_soc_ip="192.168.99.2",
    tactile_soc_port=19009,
)

# T1.5: create_hand_t1p5_*, 21 joints, default ID 1
t1p5 = OmniHand3UltraM.create_hand_t1p5_by_hcan(
    hand_type=HandType.LEFT,
    canfd_device_id=0,
    canfd_channel_id=0,
)

assert t1p3.get_revision() == H3UMRevision.T1P3
assert t1p5.get_revision() == H3UMRevision.T1P5
joint_names = t1p5.get_joint_names()  # 21 names in motor-index order
```

The ZLG and HCAN overloads also accept adapter serial numbers. Supported builds additionally expose `create_hand_by_zlgcan_tcp(...)` / `create_hand_t1p5_by_zlgcan_tcp(...)`; Linux also exposes `create_hand_socketcan(...)` / `create_hand_t1p5_socketcan(...)`. T1.3 factories accept the optional Xense endpoint through `tactile_soc_ip` and `tactile_soc_port`.

## Control and State

```python
hand.set_joint_position(joint_motor_index, posi)
position = hand.get_joint_position(joint_motor_index)
positions = hand.set_all_joint_positions(values)  # T1.3: 20; T1.5: 21
positions = hand.get_all_joint_positions()

echo = hand.set_all_active_joint_angles(angles)    # T1.3: 20; T1.5: 21
angles = hand.get_all_active_joint_angles()
all_angles = hand.get_all_joint_angles()

hand.set_control_mode(joint_motor_index, int(ControlMode.POSITION))
mode = hand.get_control_mode(joint_motor_index)
hand.set_all_control_modes(modes)
modes = hand.get_all_control_modes()
```

The binding also exposes current-threshold, velocity, temperature, current, error-report, request-interval, receive-timeout, raw tactile, resultant-force, and detail-log APIs. T1.5 additionally provides `get_temperature_of_palm()` and `get_temperature_of_power()`.

## Kinematics Solver

```python
# The one-argument constructor remains T1.3 for backward compatibility.
t1p3_solver = OmniHand3UltraMSolver(is_left_hand=True)
t1p5_solver = OmniHand3UltraMSolver(
    is_left_hand=True,
    revision=H3UMRevision.T1P5,
)
actuator = t1p5_solver.active_joint_pos_to_actuator_input(active_joint_pos)
angles = t1p5_solver.actuator_input_to_active_joint_pos(actuator)
```

Vectors contain 20 values for T1.3 or 21 values for T1.5. Angles are radians and actuator inputs use `[0, 4096]` protocol units.

## Related Documentation

- [H3UM C++ API](API_CPP_H3UM.md)
- [H3UM ROS2 interface](API_ROS2_H3UM.md)
- [Python API index](API_PYTHON.md)
- [1.1.8 to 1.1.9 migration](MIGRATION_1_1_8_TO_1_1_9.md)
