# OmniHand 3 Ultra M (H3UM) Python API

## Overview

`OmniHand3UltraM` detects hardware revisions through its factories. Applications can request automatic address and capability discovery through the same factories by passing `hand_device_id=0`.

```python
from omnihand import ControlMode, H3UMRevision, HandType, OmniHand3UltraM, OmniHand3UltraMSolver
```

## Factories

```python
hand = OmniHand3UltraM.create_hand_by_zlgcan(
    hand_type=HandType.LEFT,
    hand_device_id=0,  # Explicitly request automatic discovery.
    canfd_device_id=0,
    canfd_channel_id=0,
)
if hand is None:
    raise RuntimeError("H3UM device discovery failed")

joint_names = hand.get_joint_names()
joint_count = hand.get_num_of_joint_motors()
```

The Python default remains `hand_device_id=9`: omitting the argument or explicitly passing any nonzero ID queries DOF at that address without broadcast or address fallback. The address does not select a hardware revision. Passing `0` requests automatic address discovery: the SDK broadcasts a device-information read, falls back to address `9` if no reply arrives, then reads the standalone DOF register. A valid 20- or 21-joint reply selects the implementation; failed or unsupported replies return `None`. Automatic address discovery requires one H3UM per selected channel; on a shared bus, pass an explicit device ID.

`create_hand_by_hcan` and `create_hand_socketcan` (Linux) use the same discovery flow. ZLG/HCAN also accept adapter serial numbers. The optional final `show_data_details` argument defaults to `False`. T1.3 uses `192.168.99.2:19009` for its external tactile service by default; `hand.attach_tactile_soc(ip, 19009)` replaces the endpoint and reinitializes tactile sensing. An empty IP detaches the service. The method waits for old workers, then scans fingers asynchronously; it returns `False` on T1.5 or a failed endpoint probe.

The dedicated `create_hand_t1p5_by_zlgcan` factories have been removed; use `create_hand_by_zlgcan` for automatic revision detection. `get_revision()` remains available for diagnostics and offline model selection.

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

The binding also exposes current-threshold, velocity, temperature, current, error-report, request-interval, receive-timeout, and detail-log APIs. T1.5 additionally provides `get_temperature_of_palm()` and `get_temperature_of_power()`.

## Tactile Data

`get_tactile_sensor_data_raw(sensor)` and `get_all_tactile_sensor_data_raw()` return different data types depending on the hand object's revision:

- **T1.3**: Downsampled 3D tactile force as `TactileSensorData3DI16`, with signed 16-bit XYZ components per point in `points`. `get_tactile_sensor_resultant_force(sensor)` and `get_all_tactile_sensor_resultant_forces()` retrieve resultant forces and are supported only on T1.3.
- **T1.5**: 1D U16 tactile data as `TactileSensorDataU16`, with one `uint16_t` pressure value per point in `data_`. T1.5 does not support the 3D resultant-force methods above.

Python keeps the same raw method names; the binding selects the tactile interface by revision. C++ uses `GetTactileSensor3D()` and `GetTactileSensor1D()` respectively; see the [H3UM C++ API](API_CPP_H3UM.md).

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
