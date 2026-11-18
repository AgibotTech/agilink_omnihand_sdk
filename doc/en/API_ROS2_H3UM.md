# OmniHand 3 Ultra M (H3UM T1.3/T1.5) ROS2 Interface

> **Linux only**: the ROS2 interface is not built on Windows.

## Overview

One `omnihand_3_ultra_m_node` executable supports both revisions. The SDK detects each hand's actual revision independently:

| Revision | Active joints | Factory default device ID |
|----------|---------------|---------------------------|
| T1.3 | 20 | 9 |
| T1.5 | 21 | 1 |

The node no longer configures `revision`. Unified SDK factories detect it from the device's reported active DOF; failed detection fails initialization rather than guessing a revision. Joint names and array lengths come from the SDK; they are not hard-coded in the node.

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

After initialization, each hand node exposes a read-only `joint_names` string-array parameter containing the detected SDK joint order (20 or 21 names). Startup overrides are ignored. Scripts and `ros2_joint_cmd_demo` discover the node publishing the selected `joint_states` topic and query this parameter before sending commands. They do not wait for position feedback or send hardware queries; missing, ambiguous, invalid or timed-out metadata stops the example without sending commands. The mixed-control demos do not support the H3UM node.

## Configuration

The default file is [omnihand_3_ultra_m_node.yaml](../../../node/omnihand_node/config/omnihand_3_ultra_m_node.yaml). Supported transports are `zlgcan`, `hcan`, and Linux `socketcan`.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `hand_device_id` | int | `0` | `0` lets the SDK discover the address; 1-127 queries an explicit address while still detecting the revision |
| `connection_type` | string | `""` | `zlgcan`, `hcan`, or `socketcan`; empty skips this hand |
| `canfd_serial_number` | string | `""` | Adapter serial number; preferred over device index |
| `canfd_device_id` | int | `0` | Adapter index when no serial number is supplied |
| `canfd_channel_id` | int | `0` | CAN FD channel |
| `can_interface` | string | `can0` | SocketCAN interface |
| `request_interval_ms` | int | `-1` | `-1` keeps the SDK default |
| `frame_recv_timeout_ms` | int | `-1` | `-1` keeps the SDK default |
| `show_data_details` | bool | `false` | Enables SDK TX/RX logging |

Automatic discovery requires one hand per selected CAN channel. If no broadcast reply arrives, the SDK tries the legacy T1.3 default address 9, but still determines the revision from the active DOF reply. Specify each actual device ID when multiple hands share a channel. Replace the old `hand_device_id: -1` with `0` or an actual ID.

This example discovers the devices on two independent CAN channels:

```yaml
/**:
  ros__parameters:
    left_hand:
      hand_device_id: 0
      connection_type: "socketcan"
      can_interface: "can0"

    right_hand:
      hand_device_id: 0
      connection_type: "socketcan"
      can_interface: "can1"
```

## Launch and Examples

```bash
ros2 launch omnihand_node omnihand_3_ultra_m_node.launch.py

# Position control: automatically query the detected joint count.
python3 scripts/omnihand_3_ultra_m/joint_cmd.py left h3u_m

# Set all control modes or current thresholds for the connected revision.
python3 scripts/omnihand_3_ultra_m/joint_control_mode_pub.py 0 left h3u_m
python3 scripts/omnihand_3_ultra_m/joint_current_threshold_pub.py 500 left h3u_m

ros2 run omnihand_ros2_demo ros2_joint_cmd_demo left h3u_m
```

## Related Documentation

- [Unified ROS2 interface](API_ROS2.md)
- [H3UM C++ API](API_CPP_H3UM.md)
- [H3UM Python API](API_PYTHON_H3UM.md)
