# OmniHand 3 Ultra M (H3UM T1.3/T1.5) ROS2 Interface

> **Linux only**: the ROS2 interface is not built on Windows.

## Overview

One `omnihand_3_ultra_m_node` executable supports both revisions. Each hand independently selects its revision in YAML:

| Revision | Active joints | Default device ID | `revision` value |
|----------|---------------|-------------------|------------------|
| T1.3 | 20 | 9 | `t1p3` |
| T1.5 | 21 | 1 | `t1p5` |

An absent or unrecognized `revision` falls back to T1.3 and logs one warning for backward compatibility. Joint names and array lengths come from the SDK; they are not hard-coded in the node.

## Topics

All topics use `/h3u_m/<side>/`, where `<side>` is `left` or `right`.

| Topic | Message type | Direction | Description |
|-------|--------------|-----------|-------------|
| `joint_cmd` | `sensor_msgs/JointState` | Subscribe | Active-joint angles in radians; accepts SDK joint-name order or a complete matching `name[]` mapping |
| `joint_states` | `sensor_msgs/JointState` | Publish | Echoed active-joint angles and SDK joint names |
| `joint_cmd_source` | `std_msgs/Int8` | Subscribe | `0=NORMAL`, `1=ANGLE`, `2=POSITION` |
| `joint_angle_cmd` | `sensor_msgs/JointState` | Subscribe | Angle command when source is `1` |
| `joint_position_cmd` | `std_msgs/Int16MultiArray` | Subscribe | Raw motor positions when source is `2` |
| `joint_position_states` | `omnihand_node_msgs/Int16MultiArrayStamped` | Publish | Raw position readback |
| `joint_error_cmd` / `joint_error_states` | `Empty` / `Int16MultiArrayStamped` | Trigger / publish | Per-joint error reports |
| `joint_temperature_cmd` / `joint_temperature_states` | `Empty` / `Int16MultiArrayStamped` | Trigger / publish | Per-joint temperatures |
| `joint_current_cmd` / `joint_current_states` | `Empty` / `Int16MultiArrayStamped` | Trigger / publish | Per-joint currents |
| `joint_control_mode_cmd` / `joint_control_mode_states` | `Int8MultiArray` / `Int8MultiArrayStamped` | Command / publish | Per-joint control modes |
| `joint_current_threshold_cmd` / `joint_current_threshold_states` | `Int16MultiArray` / `Int16MultiArrayStamped` | Command / publish | Per-joint current thresholds |

All full-hand arrays must contain 20 elements for T1.3 or 21 for T1.5. Incorrect lengths are rejected with a warning. The current H3UM node does not publish tactile topics; use the C++ or Python SDK tactile APIs directly.

## Configuration

The default file is [omnihand_3_ultra_m_node.yaml](../../../node/omnihand_node/config/omnihand_3_ultra_m_node.yaml). Supported transports are `zlgcan`, `hcan`, and Linux `socketcan`.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `revision` | string | `t1p3` | `t1p3` or `t1p5`; unknown values fall back to T1.3 |
| `hand_device_id` | int | `-1` | `-1` selects the revision default; explicit IDs must be 1-127 |
| `connection_type` | string | `""` | `zlgcan`, `hcan`, or `socketcan`; empty skips this hand |
| `canfd_serial_number` | string | `""` | Adapter serial number; preferred over device index |
| `canfd_device_id` | int | `0` | Adapter index when no serial number is supplied |
| `canfd_channel_id` | int | `0` | CAN FD channel |
| `can_interface` | string | `can0` | SocketCAN interface |
| `request_interval_ms` | int | `-1` | `-1` keeps the SDK default |
| `frame_recv_timeout_ms` | int | `-1` | `-1` keeps the SDK default |
| `show_data_details` | bool | `false` | Enables SDK TX/RX logging |

Use explicit revision defaults in deployment YAML for readability:

```yaml
/**:
  ros__parameters:
    left_hand:
      revision: "t1p3"
      hand_device_id: 9
      connection_type: "socketcan"
      can_interface: "can0"

    right_hand:
      revision: "t1p5"
      hand_device_id: 1
      connection_type: "socketcan"
      can_interface: "can1"
```

## Launch and Examples

```bash
ros2 launch omnihand_node omnihand_3_ultra_m_node.launch.py

# Position control: final argument selects 20/21-joint example payloads.
python3 scripts/omnihand_3_ultra_m/joint_cmd.py left h3u_m t1p5

# Set all control modes or current thresholds for T1.5.
python3 scripts/omnihand_3_ultra_m/joint_control_mode_pub.py 0 left h3u_m t1p5
python3 scripts/omnihand_3_ultra_m/joint_current_threshold_pub.py 500 left h3u_m t1p5
```

## Related Documentation

- [Unified ROS2 interface](API_ROS2.md)
- [H3UM C++ API](API_CPP_H3UM.md)
- [H3UM Python API](API_PYTHON_H3UM.md)
