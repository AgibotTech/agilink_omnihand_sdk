# OmniHand 3 Ultra M（H3UM T1.3/T1.5）ROS2 接口

> **仅支持 Linux**：Windows 不构建 ROS2 接口。

## 概述

同一个 `omnihand_3_ultra_m_node` 可支持两个版本，并允许左右手分别配置版本：

| 版本 | 主动关节数 | 默认设备 ID | `revision` 配置值 |
|------|-----------|-------------|-------------------|
| T1.3 | 20 | 9 | `t1p3` |
| T1.5 | 21 | 1 | `t1p5` |

未配置或无法识别 `revision` 时，为向前兼容会回退到 T1.3，并打印一次警告。关节名称和数组长度均来自 SDK，不在节点内写死。

## Topics

所有 Topic 使用 `/h3u_m/<side>/` 前缀，`<side>` 为 `left` 或 `right`。

| Topic | 消息类型 | 方向 | 说明 |
|-------|---------|------|------|
| `joint_cmd` | `sensor_msgs/JointState` | 订阅 | 主动关节弧度；可按 SDK 默认顺序发送，也可通过完整匹配的 `name[]` 映射 |
| `joint_states` | `sensor_msgs/JointState` | 发布 | 控制回读的主动关节角及 SDK 关节名称 |
| `joint_cmd_source` | `std_msgs/Int8` | 订阅 | `0=NORMAL`、`1=ANGLE`、`2=POSITION` |
| `joint_angle_cmd` | `sensor_msgs/JointState` | 订阅 | 命令源为 `1` 时的角度指令 |
| `joint_position_cmd` | `std_msgs/Int16MultiArray` | 订阅 | 命令源为 `2` 时的电机原始位置 |
| `joint_position_states` | `omnihand_node_msgs/Int16MultiArrayStamped` | 发布 | 原始位置回读 |
| `joint_error_cmd` / `joint_error_states` | `Empty` / `Int16MultiArrayStamped` | 触发 / 发布 | 各关节错误报告 |
| `joint_temperature_cmd` / `joint_temperature_states` | `Empty` / `Int16MultiArrayStamped` | 触发 / 发布 | 各关节温度 |
| `joint_current_cmd` / `joint_current_states` | `Empty` / `Int16MultiArrayStamped` | 触发 / 发布 | 各关节电流 |
| `joint_control_mode_cmd` / `joint_control_mode_states` | `Int8MultiArray` / `Int8MultiArrayStamped` | 命令 / 发布 | 各关节控制模式 |
| `joint_current_threshold_cmd` / `joint_current_threshold_states` | `Int16MultiArray` / `Int16MultiArrayStamped` | 命令 / 发布 | 各关节电流阈值 |

所有整手数组在 T1.3 下必须为 20 项，在 T1.5 下必须为 21 项；长度错误时节点会打印警告并忽略命令。目前 H3UM 节点不发布触觉 Topic，触觉数据请直接使用 C++ 或 Python SDK 接口。

## 配置

默认配置文件为 [omnihand_3_ultra_m_node.yaml](../../../node/omnihand_node/config/omnihand_3_ultra_m_node.yaml)。支持 `zlgcan`、`hcan` 和 Linux `socketcan`。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `revision` | string | `t1p3` | `t1p3` 或 `t1p5`；未知值回退 T1.3 |
| `hand_device_id` | int | `-1` | `-1` 按版本选择默认 ID；显式 ID 范围为 1-127 |
| `connection_type` | string | `""` | `zlgcan`、`hcan` 或 `socketcan`；为空时跳过该手 |
| `canfd_serial_number` | string | `""` | 适配器序列号，优先于设备索引 |
| `canfd_device_id` | int | `0` | 未提供序列号时使用的适配器索引 |
| `canfd_channel_id` | int | `0` | CAN FD 通道 |
| `can_interface` | string | `can0` | SocketCAN 接口名 |
| `request_interval_ms` | int | `-1` | `-1` 保持 SDK 默认值 |
| `frame_recv_timeout_ms` | int | `-1` | `-1` 保持 SDK 默认值 |
| `show_data_details` | bool | `false` | 是否打印 SDK 收发日志 |

实际部署建议像示例文件一样显式写出各版本默认 ID，阅读更直观：

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

## 启动与示例

```bash
ros2 launch omnihand_node omnihand_3_ultra_m_node.launch.py

# 位置控制：最后一个参数决定示例发送 20 项还是 21 项。
python3 scripts/omnihand_3_ultra_m/joint_cmd.py left h3u_m t1p5

# 为 T1.5 设置整手控制模式或电流阈值。
python3 scripts/omnihand_3_ultra_m/joint_control_mode_pub.py 0 left h3u_m t1p5
python3 scripts/omnihand_3_ultra_m/joint_current_threshold_pub.py 500 left h3u_m t1p5
```

## 相关文档

- [ROS2 统一接口](API_ROS2.md)
- [H3UM C++ API](API_CPP_H3UM.md)
- [H3UM Python API](API_PYTHON_H3UM.md)
