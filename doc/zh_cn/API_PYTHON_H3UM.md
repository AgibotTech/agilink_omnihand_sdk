# OmniHand 3 Ultra M（H3UM）Python API

## 概述

`OmniHand3UltraM` 通过工厂函数自动识别硬件版本。需要自动发现设备地址和硬件能力时，显式传入 `hand_device_id=0`。

```python
from omnihand import ControlMode, H3UMRevision, HandType, OmniHand3UltraM, OmniHand3UltraMSolver
```

## 工厂函数

```python
hand = OmniHand3UltraM.create_hand_by_zlgcan(
    hand_type=HandType.LEFT,
    hand_device_id=0,  # 显式启用自动发现。
    canfd_device_id=0,
    canfd_channel_id=0,
)
if hand is None:
    raise RuntimeError("H3UM device discovery failed")

joint_names = hand.get_joint_names()
joint_count = hand.get_num_of_joint_motors()
```

Python 默认值仍为 `hand_device_id=9`：省略此参数或显式传入任意非零 ID 时，直接读取该地址的自由度，不发送广播或回退其他地址。设备地址不决定硬件版本。传入 `0` 时启用地址自动发现：SDK 广播读取设备信息，无回复时回退地址 `9`，再单独读取自由度寄存器。有效的 20 或 21 关节回复决定对应实现，探测失败或自由度不受支持时返回 `None`。地址自动发现适用于所选通道只有一只 H3UM 的场景；同一总线上有多台设备时，请显式传入目标设备 ID。

`create_hand_by_hcan`、`create_hand_by_zlgcan_tcp`（相应平台）和 `create_hand_socketcan`（Linux）使用相同的识别流程。ZLG/HCAN 也支持按适配器序列号连接。末尾可选参数 `show_data_details` 默认为 `False`。T1.3 的外部触觉服务默认地址为 `192.168.99.2:19009`；`hand.attach_tactile_soc(ip, 19009)` 可更换地址并重新初始化，IP 为空时断开服务。该方法等待旧线程退出，五指扫描异步执行；T1.5 或端点探测失败时返回 `False`。

已移除专用的 `create_hand_t1p5_by_zlgcan` 创建接口，请使用 `create_hand_by_zlgcan` 自动识别版本。`get_revision()` 仍可用于诊断和离线模型选择。

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

绑定还提供电流阈值、速度、温度、电流、错误报告、请求间隔、接收超时和详细日志接口。T1.5 还提供 `get_temperature_of_palm()` 与 `get_temperature_of_power()`。

## 触觉数据

`get_tactile_sensor_data_raw(sensor)` 与 `get_all_tactile_sensor_data_raw()` 根据手对象的版本返回不同类型的数据：

- **T1.3**：3D 触觉力降采样数据，类型为 `TactileSensorData3DI16`，`points` 保存各点有符号 16 位 XYZ 分量。`get_tactile_sensor_resultant_force(sensor)` 与 `get_all_tactile_sensor_resultant_forces()` 用于获取合力，仅支持 T1.3。
- **T1.5**：1D U16 触觉数据，类型为 `TactileSensorDataU16`，`data_` 保存每点的 `uint16_t` 压力值。T1.5 不支持上述 3D 合力接口。

Python 的 Raw 方法名保持一致，绑定层按版本选择对应的触觉接口。C++ 分别使用 `GetTactileSensor3D()` 和 `GetTactileSensor1D()`，详见 [H3UM C++ API](API_CPP_H3UM.md)。

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
