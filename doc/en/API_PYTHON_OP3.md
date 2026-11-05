# OmniPicker 3 (OP3) Python API

## Overview

`OmniPicker3` is the Python interface for the 1-DOF OmniPicker 3 gripper. It
supports ZLG CAN FD, HCAN, ZLG CAN TCP, Linux SocketCAN, and RS485.

The Python surface intentionally excludes persistent or maintenance operations:

- device-ID writes;
- calibration and zeroing;
- firmware update;
- persistent parameter and threshold writes;
- unsupported direct PV/PVT APIs.

Position, stroke, gesture, gentle-grip, PT control, status queries, and tactile
reads are available. Control calls can move the gripper.

## Import

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

`ProductType.OMNI_PICKER_3` has value `5`.

## Factory Methods

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

ZLG CAN TCP is available on Windows and Linux x64. SocketCAN is available only
on Linux. OP3 does not distinguish left and right hands; use
`HandType.UNKNOWN` unless an application needs to retain a side label.

## Initialization and Information

```python
picker = OmniPicker3.create_hand_by_hcan(
    hand_type=HandType.UNKNOWN,
    hand_device_id=OmniPicker3.kDefaultHandDeviceId,
    canfd_device_id=0,
    canfd_channel_id=0,
)

if not picker.init():
    raise RuntimeError("OP3 initialization failed")

product = picker.get_product_type()
vendor = picker.get_vendor_info()
device = picker.get_device_info()
device_id = picker.get_hand_device_id_by_broadcast()
```

`get_hand_device_id_by_broadcast()` only discovers and caches the current ID; it
does not write device configuration. Device-ID write methods are not exposed.

## Product Metadata and Ranges

```python
OmniPicker3.kDegreesOfActiveFreedom  # 1
OmniPicker3.kDefaultHandDeviceId     # 1

picker.get_num_of_joint_motors()     # 1
picker.get_doa()                     # 1
picker.get_dop()                     # 0
picker.get_joint_names()             # ["joint1"]

OmniPicker3.get_min_max_motor_position()     # 0..4095 ticks
OmniPicker3.get_min_max_mix_ctrl_torque()    # 0..6500 mA, default 3000
OmniPicker3.get_min_max_stroke()             # -500..11200, unit 0.01 mm
OmniPicker3.get_min_max_stroke_mix_speed()   # 0..5000 rpm, default 3000
OmniPicker3.get_min_max_mix_ctrl_current()   # 0..3500 mA, default 3000
```

Range methods return `Int16Bound` or `Int16Range` objects.

## Position Control

Motor indices are 1-based.

```python
actual = picker.set_joint_position(1, 2048)
position = picker.get_joint_position(1)

actual_all = picker.set_all_joint_positions([2048])
positions = picker.get_all_joint_positions()
```

The common 0x24 status APIs are also inherited:

```python
single = picker.set_joint_position_with_status(1, 2048)
single = picker.get_joint_position_with_status(1)
all_status = picker.set_all_joint_positions_with_status([2048])
all_status = picker.get_all_joint_positions_with_status()
```

They return position feedback together with the running and error codes.

## Stroke and Gentle-Grip Control

```python
# 50.00 mm; firmware supplies its default speed/current.
stroke = picker.mix_control_by_stroke(5000)

# 50.00 mm, 3000 rpm, 2000 mA.
stroke = picker.mix_control_by_stroke(5000, 3000, 2000)

# Enter gentle-grip mode with the firmware default or a 1000 mA limit.
raw_reply = picker.set_gentle_grip()
raw_reply = picker.set_gentle_grip(1000)
```

The stroke reply is the actual stroke sampled when the command arrives, not the
requested target. Values can be slightly negative near the fully closed end.
Out-of-range arguments are clamped to the published ranges.

The protocol has no command for leaving gentle-grip mode completely. Position
or stroke commands can reopen the gripper, but a power cycle is required to
clear the mode.

## Gestures

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

## PT Mixed Control

OP3 supports position + current only. The Python name retains the SDK's `PT`
terminology; the torque field is motor current in mA.

```python
single = picker.mix_control_by_pt(1, 2048, 1000)
all_feedback = picker.mix_control_by_pt([2048], [1000])
```

Direct `mix_control_by_pv()` and `mix_control_by_pvt()` methods are not exposed
for OP3. Generic status methods inherited from `OmniHand` must still be used
according to product capability; OP3 applications should use PT rather than PVT.

```python
status = picker.mix_control_by_pt_with_status(1, 2048, 1000)
all_status = picker.mix_control_by_pt_with_status([2048], [1000])
```

`mix_control_with_status()` is also available for callers that construct
`MixCtrl` entries directly.

OP3 has no kinematics solver. The common
`get_all_joint_angles_from_motor_positions()` method therefore returns the
no-solver fallback and should not be used for OP3 motion calculations.

## Reports and Detailed Status

```python
error = picker.get_error_report(1)
errors = picker.get_all_error_reports()

temperature = picker.get_temperature_report(1)
temperatures = picker.get_all_temperature_reports()

current = picker.get_current_report(1)
currents = picker.get_all_current_reports()

hand_detail = picker.get_all_joint_detail_info()
joint_detail = picker.get_joint_detail_info(0)  # detail index is 0-based
```

Detailed 0x26 data depends on backend and firmware support; unsupported queries
return default structures after the SDK reports the communication failure.

## U16 Tactile Data

```python
if not picker.init_tactile_points_map():
    raise RuntimeError("OP3 tactile initialization failed")

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

`TactileSensorDataU16.sensor_id` is a `Finger`; `data` is a list of unsigned
16-bit samples. Prefer `get_all_tactile_sensor_data_raw()` when reading all
regions. `get_tactile_sensor_data()` requests downsampled register 0x05 data;
current firmware may return an empty payload.

## Communication and Logging

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

RS485 ignores the frame-send timeout and returns `0` from its getter.
`set_broadcast()` changes the request addressing mode in the SDK and does not
write a new device ID.

## Hardware Pytest

The unified test intentionally does not modify the device ID or call
calibration, zeroing, OTA, or persistent parameter APIs.

```bash
# Default ZLG CAN FD
python python/test/test_omnipicker_3.py

# HCAN
python python/test/test_omnipicker_3.py -d hcan -i 0 -c 0

# RS485 on Windows
python python/test/test_omnipicker_3.py -d rs485 -P COM7 -b 460800
```

The test requires connected hardware and fails if initialization does not
succeed. Motion-control cases move the gripper.

## Related Documentation

- [Python API index](API_PYTHON.md)
- [OP3 C++ API](API_CPP_OP3.md)
- [OP3 ROS 2 interface](API_ROS2_OP3.md)

