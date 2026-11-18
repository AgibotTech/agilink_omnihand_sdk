# OmniHand 3 Ultra M（H3UM T1.3/T1.5）ROS2 接口

> **仅支持 Linux**：Windows 不构建 ROS2 接口。

## 概述

同一个 `omnihand_3_ultra_m_node` 支持两个版本，由 SDK 分别识别左右手的实际版本：

| 版本 | 主动关节数 | 出厂默认设备 ID |
|------|-----------|-----------------|
| T1.3 | 20 | 9 |
| T1.5 | 21 | 1 |

节点不再配置 `revision`，统一通过 SDK 创建接口按设备返回的主动自由度识别版本。识别失败则初始化失败，不会猜测版本。关节名称和数组长度均来自 SDK，不在节点内写死。

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

初始化后，每只手的节点提供只读字符串数组参数 `joint_names`，内容为 SDK 识别的关节顺序（20 或 21 个名称），忽略启动参数覆盖。scripts 和 `ros2_joint_cmd_demo` 会发现目标 `joint_states` Topic 的发布节点，发送指令前查询此参数，无需等待位置反馈，也不增加硬件查询；元数据缺失、来源不唯一、内容无效或查询超时都会终止示例，不发送指令。混控 demo 不支持当前 H3UM 节点。

## 配置

默认配置文件为 [omnihand_3_ultra_m_node.yaml](../../../node/omnihand_node/config/omnihand_3_ultra_m_node.yaml)。支持 `zlgcan`、`hcan` 和 Linux `socketcan`。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `hand_device_id` | int | `0` | `0` 由 SDK 自动发现地址；1-127 查询指定地址，仍自动识别版本 |
| `connection_type` | string | `""` | `zlgcan`、`hcan` 或 `socketcan`；为空时跳过该手 |
| `canfd_serial_number` | string | `""` | 适配器序列号，优先于设备索引 |
| `canfd_device_id` | int | `0` | 未提供序列号时使用的适配器索引 |
| `canfd_channel_id` | int | `0` | CAN FD 通道 |
| `can_interface` | string | `can0` | SocketCAN 接口名 |
| `request_interval_ms` | int | `-1` | `-1` 保持 SDK 默认值 |
| `frame_recv_timeout_ms` | int | `-1` | `-1` 保持 SDK 默认值 |
| `show_data_details` | bool | `false` | 是否打印 SDK 收发日志 |

自动发现要求所选 CAN 通道只有一只手。没有广播应答时，SDK 会尝试旧版 T1.3 默认地址 9，但版本仍由主动自由度查询结果确定。同一通道有多只手时，请分别指定实际 ID。原来的 `hand_device_id: -1` 应改为 `0` 或实际 ID。

以下示例在两个独立 CAN 通道上自动发现设备：

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

## 启动与示例

```bash
ros2 launch omnihand_node omnihand_3_ultra_m_node.launch.py

# 位置控制：自动查询已识别的关节数。
python3 scripts/omnihand_3_ultra_m/joint_cmd.py left h3u_m

# 为当前连接的版本设置整手控制模式或电流阈值。
python3 scripts/omnihand_3_ultra_m/joint_control_mode_pub.py 0 left h3u_m
python3 scripts/omnihand_3_ultra_m/joint_current_threshold_pub.py 500 left h3u_m

ros2 run omnihand_ros2_demo ros2_joint_cmd_demo left h3u_m
```

## 相关文档

- [ROS2 统一接口](API_ROS2.md)
- [H3UM C++ API](API_CPP_H3UM.md)
- [H3UM Python API](API_PYTHON_H3UM.md)
