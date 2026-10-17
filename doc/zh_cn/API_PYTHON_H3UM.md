# OmniHand 3 Ultra M（H3UM T1.3/T1.5）Python API

## 概述

Python 使用同一个 `OmniHand3UltraM` 类支持两个版本。T1.3 有 20 个主动关节，默认设备 ID 为 `9`；T1.5 有 21 个主动关节，默认设备 ID 为 `1`。

```python
from omnihand import (
    ControlMode,
    H3UMRevision,
    HandType,
    OmniHand3UltraM,
    OmniHand3UltraMSolver,
)
```

## 工厂函数

```python
# T1.3：create_hand_by_*，20 关节，默认 ID 9
t1p3 = OmniHand3UltraM.create_hand_by_zlgcan(
    hand_type=HandType.LEFT,
    canfd_device_id=0,
    canfd_channel_id=0,
    tactile_soc_ip="192.168.99.2",
    tactile_soc_port=19009,
)

# T1.5：create_hand_t1p5_*，21 关节，默认 ID 1
t1p5 = OmniHand3UltraM.create_hand_t1p5_by_hcan(
    hand_type=HandType.LEFT,
    canfd_device_id=0,
    canfd_channel_id=0,
)

assert t1p3.get_revision() == H3UMRevision.T1P3
assert t1p5.get_revision() == H3UMRevision.T1P5
joint_names = t1p5.get_joint_names()  # 按电机索引排列的 21 个名称
```

ZLG 和 HCAN 也提供按适配器序列号创建的重载。相应平台还提供 `create_hand_by_zlgcan_tcp(...)` / `create_hand_t1p5_by_zlgcan_tcp(...)`；Linux 还提供 `create_hand_socketcan(...)` / `create_hand_t1p5_socketcan(...)`。T1.3 工厂可通过 `tactile_soc_ip` 和 `tactile_soc_port` 配置 Xense 端点。

## 控制与状态

```python
hand.set_joint_position(joint_motor_index, posi)
position = hand.get_joint_position(joint_motor_index)
positions = hand.set_all_joint_positions(values)  # T1.3：20；T1.5：21
positions = hand.get_all_joint_positions()

echo = hand.set_all_active_joint_angles(angles)    # T1.3：20；T1.5：21
angles = hand.get_all_active_joint_angles()
all_angles = hand.get_all_joint_angles()

hand.set_control_mode(joint_motor_index, int(ControlMode.POSITION))
mode = hand.get_control_mode(joint_motor_index)
hand.set_all_control_modes(modes)
modes = hand.get_all_control_modes()
```

绑定还提供电流阈值、速度、温度、电流、错误报告、请求间隔、接收超时、原始触觉、触觉合力和详细日志接口。T1.5 还提供 `get_temperature_of_palm()` 与 `get_temperature_of_power()`。

## 运动学求解器

```python
# 单参数构造为保持兼容，仍默认使用 T1.3。
t1p3_solver = OmniHand3UltraMSolver(is_left_hand=True)
t1p5_solver = OmniHand3UltraMSolver(
    is_left_hand=True,
    revision=H3UMRevision.T1P5,
)
actuator = t1p5_solver.active_joint_pos_to_actuator_input(active_joint_pos)
angles = t1p5_solver.actuator_input_to_active_joint_pos(actuator)
```

T1.3 向量包含 20 个值，T1.5 包含 21 个值；角度单位为弧度，执行器输入使用 `[0, 4096]` 协议值。

## 相关文档

- [H3UM C++ API](API_CPP_H3UM.md)
- [H3UM ROS2 接口](API_ROS2_H3UM.md)
- [Python API 总览](API_PYTHON.md)
- [从 1.1.8 迁移到 1.1.9](MIGRATION_1_1_8_TO_1_1_9.md)
