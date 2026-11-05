# OmniPicker 3（OP3）Python API

## 概述

`OmniPicker3` 是 1 自由度 OmniPicker 3 夹爪的 Python 接口，支持 ZLG CAN
FD、HCAN、ZLG CAN TCP、Linux SocketCAN 和 RS485。

Python 接口有意不开放以下持久化或维护类能力：

- 修改设备 ID；
- 标定与零点设置；
- 固件升级；
- 持久化参数和阈值写入；
- OP3 不支持的直接 PV/PVT 接口。

位置、行程、手势、轻柔夹持、PT 控制、状态查询和触觉读取接口可以使用。
控制类接口会使夹爪运动。

## 导入

```python
from omnihand import (
    Finger,
    HandType,
    OmniPicker3,
    OmniPicker3Gesture,
    ProductType,
    TactileSensorDataU16,
)
```

`ProductType.OMNI_PICKER_3` 的值为 `5`。

## 工厂方法

```python
OmniPicker3.create_hand_by_zlgcan(
    hand_type=HandType.UNKNOWN,
    hand_device_id=1,
    canfd_device_id=0,
    canfd_channel_id=0,
)

OmniPicker3.create_hand_by_zlgcan(
    hand_type,
    hand_device_id,
    usbcanfd_serial_number,
    canfd_channel_id=0,
)

OmniPicker3.create_hand_by_hcan(
    hand_type=HandType.UNKNOWN,
    hand_device_id=1,
    canfd_device_id=0,
    canfd_channel_id=0,
)

OmniPicker3.create_hand_by_hcan(
    hand_type,
    hand_device_id,
    hcan_serial_number,
    canfd_channel_id=0,
)

OmniPicker3.create_hand_by_zlgcan_tcp(
    hand_type, hand_device_id, host, port, canfd_channel_id=0
)

OmniPicker3.create_hand_socketcan(
    hand_type=HandType.UNKNOWN,
    hand_device_id=1,
    can_interface="can0",
)

OmniPicker3.create_hand_by_rs485(
    hand_type=HandType.UNKNOWN,
    hand_device_id=1,
    serial_port="/dev/ttyUSB0",
    baud_rate=460800,
)
```

ZLG CAN TCP 支持 Windows 和 Linux x64；SocketCAN 仅支持 Linux。OP3
不区分左右手，除非应用需要保留侧别标签，否则使用 `HandType.UNKNOWN`。

## 初始化与设备信息

```python
picker = OmniPicker3.create_hand_by_hcan(
    hand_type=HandType.UNKNOWN,
    hand_device_id=OmniPicker3.kDefaultHandDeviceId,
    canfd_device_id=0,
    canfd_channel_id=0,
)

if not picker.init():
    raise RuntimeError("OP3 初始化失败")

product = picker.get_product_type()
vendor = picker.get_vendor_info()
device = picker.get_device_info()
device_id = picker.get_hand_device_id_by_broadcast()
```

`get_hand_device_id_by_broadcast()` 只查询并缓存当前 ID，不会写入设备配置。
Python API 不开放设备 ID 写入方法。

## 产品信息与范围

```python
OmniPicker3.kDegreesOfActiveFreedom  # 1
OmniPicker3.kDefaultHandDeviceId     # 1

picker.get_num_of_joint_motors()     # 1
picker.get_doa()                     # 1
picker.get_dop()                     # 0
picker.get_joint_names()             # ["joint1"]

OmniPicker3.get_min_max_motor_position()     # 0..4095 ticks
OmniPicker3.get_min_max_mix_ctrl_torque()    # 0..6500 mA，默认 3000
OmniPicker3.get_min_max_stroke()             # -500..11200，单位 0.01 mm
OmniPicker3.get_min_max_stroke_mix_speed()   # 0..5000 rpm，默认 3000
OmniPicker3.get_min_max_mix_ctrl_current()   # 0..3500 mA，默认 3000
```

范围方法返回 `Int16Bound` 或 `Int16Range` 对象。

## 位置控制

电机索引从 1 开始。

```python
actual = picker.set_joint_position(1, 2048)
position = picker.get_joint_position(1)

actual_all = picker.set_all_joint_positions([2048])
positions = picker.get_all_joint_positions()
```

同时继承公共的 0x24 状态接口：

```python
single = picker.set_joint_position_with_status(1, 2048)
single = picker.get_joint_position_with_status(1)
all_status = picker.set_all_joint_positions_with_status([2048])
all_status = picker.get_all_joint_positions_with_status()
```

返回结果包含位置反馈、运行码和错误码。

## 行程与轻柔夹持

```python
# 50.00 mm；速度和电流使用固件默认值。
stroke = picker.mix_control_by_stroke(5000)

# 50.00 mm、3000 rpm、2000 mA。
stroke = picker.mix_control_by_stroke(5000, 3000, 2000)

# 进入轻柔夹持，分别使用固件默认限制和 1000 mA 限制。
raw_reply = picker.set_gentle_grip()
raw_reply = picker.set_gentle_grip(1000)
```

行程返回值是指令到达时采样的实际行程，不是目标值；夹爪完全闭合附近可能返回
轻微负值。超出范围的参数会被限制到公开范围内。

协议没有完全退出轻柔夹持模式的命令。位置或行程指令可以重新张开夹爪，但彻底
清除该模式需要给设备断电重启。

## 预设手势

```python
class OmniPicker3Gesture(IntEnum):
    OMNIPICKER_3_GESTURE_ZERO = 0
    OMNIPICKER_3_GESTURE_HALF_OPEN = 1
    OMNIPICKER_3_GESTURE_FULL_OPEN = 2

target = picker.get_hand_gesture(
    OmniPicker3Gesture.OMNIPICKER_3_GESTURE_HALF_OPEN
)
picker.set_hand_gesture(
    OmniPicker3Gesture.OMNIPICKER_3_GESTURE_HALF_OPEN
)
```

## PT 混合控制

OP3 仅支持位置 + 电流。Python 名称沿用 SDK 的 `PT` 术语，其中 torque
字段实际表示电机电流，单位为 mA。

```python
single = picker.mix_control_by_pt(1, 2048, 1000)
all_feedback = picker.mix_control_by_pt([2048], [1000])
```

OP3 不开放直接 `mix_control_by_pv()` 和 `mix_control_by_pvt()`。从公共基类继承
的状态接口仍须按产品能力使用；OP3 应使用 PT，不应使用 PVT。

```python
status = picker.mix_control_by_pt_with_status(1, 2048, 1000)
all_status = picker.mix_control_by_pt_with_status([2048], [1000])
```

需要自行构造 `MixCtrl` 项目时，也可以使用 `mix_control_with_status()`。

OP3 没有运动学求解器。公共的
`get_all_joint_angles_from_motor_positions()` 因此只返回无求解器时的默认结果，
不应将其用于 OP3 运动计算。

## 状态报告与详细信息

```python
error = picker.get_error_report(1)
errors = picker.get_all_error_reports()

temperature = picker.get_temperature_report(1)
temperatures = picker.get_all_temperature_reports()

current = picker.get_current_report(1)
currents = picker.get_all_current_reports()

hand_detail = picker.get_all_joint_detail_info()
joint_detail = picker.get_joint_detail_info(0)  # 详细信息索引从 0 开始
```

0x26 详细信息取决于通信后端与固件支持情况。不支持时，SDK 报告通信失败并返回
默认结构。

## U16 触觉数据

```python
if not picker.init_tactile_points_map():
    raise RuntimeError("OP3 触觉初始化失败")

sensors = picker.get_sensor_order()
count = picker.get_num_of_tactile_sensors()

for sensor in sensors:
    points = picker.get_num_of_tactile_points(sensor)
    datum_size = picker.get_len_of_tactile_datum(sensor)
    frame_count = picker.get_num_of_replied_tactile_frames(sensor)
    serial_number = picker.get_sn_of_tactile_sensor(sensor)
    raw = picker.get_tactile_sensor_data_raw(sensor)

all_raw = picker.get_all_tactile_sensor_data_raw()
```

`TactileSensorDataU16.sensor_id` 是 `Finger`，`data` 是无符号 16 位采样值列表。
读取全部区域时优先使用 `get_all_tactile_sensor_data_raw()`。
`get_tactile_sensor_data()` 查询降采样寄存器 0x05，当前固件可能返回空数据。

## 通信与日志

```python
picker.set_request_interval(5)
picker.get_request_interval()
picker.set_frame_recv_timeout(200)
picker.get_frame_recv_timeout()
picker.set_frame_send_timeout(200)
picker.get_frame_send_timeout()
picker.set_broadcast(True)
picker.set_broadcast(False)
picker.show_data_details(True)
```

RS485 不使用帧发送超时，setter 不执行操作，getter 返回 `0`。
`set_broadcast()` 只切换 SDK 请求使用的寻址方式，不会写入新的设备 ID。

## 硬件 pytest

统一测试不会修改设备 ID，也不会调用标定、清零、OTA 或持久化参数接口。

```bash
# 默认 ZLG CAN FD
python python/test/test_omnipicker_3.py

# HCAN
python python/test/test_omnipicker_3.py -d hcan -i 0 -c 0

# Windows RS485
python python/test/test_omnipicker_3.py -d rs485 -P COM7 -b 460800
```

测试要求连接真实硬件，初始化失败会直接报错；运动控制用例会使夹爪运动。

## 相关文档

- [Python API 总览](API_PYTHON.md)
- [OP3 C++ API](API_CPP_OP3.md)
- [OP3 ROS 2 接口](API_ROS2_OP3.md)

