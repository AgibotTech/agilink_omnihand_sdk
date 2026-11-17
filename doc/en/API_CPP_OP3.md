# OmniPicker 3 (OP3) C++ API

## Overview

**OmniPicker 3 (OP3)** is a 1-DOF gripper supporting ZLG CAN FD, HCAN, SocketCAN, and RS485. Include `omnihand/omnipicker_3.h`; the former `omni_picker_3.h` name is no longer public.

## C++ Demo

All transports share [demo_omnipicker_3.cc](../../../cpp/demo/omnipicker_3/demo_omnipicker_3.cc), built as `demo_omnipicker_3`:

```shell
./demo_omnipicker_3 -d zlgcan -i 0 -c 0 --device-id 1
./demo_omnipicker_3 -d hcan --serial ADAPTER_SERIAL
./demo_omnipicker_3 -d socketcan --can-if can0
./demo_omnipicker_3 -d rs485 -p COM5
```

Windows executables use the `.exe` suffix; Linux serial ports can use `/dev/ttyUSB0`. SocketCAN is Linux-only. Use `--help` for all options. Demos cover normal control and information queries; they do not update firmware, calibrate, set zero offsets, write serial numbers, or write factory parameters.

The demo covers motor position control, a 50 mm stroke with speed/current limits, gentle gripping with a 1000 mA limit, and motor temperature/current/error and both tactile-region queries. It reads sensors after gripping and opens again at the end. Tactile output previews the first 8 points; the returned vector contains the complete data.

See [CMakeLists.txt](../../../cpp/demo/omnipicker_3/CMakeLists.txt) for build instructions.

## Factory Methods

```cpp
static std::unique_ptr<OmniPicker3> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0);

static std::unique_ptr<OmniPicker3> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0);

static std::unique_ptr<OmniPicker3> createHandByRs485(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& uart_port, int32_t baudrate = 460800);
```

The ZLG and HCAN factories also have serial-number overloads. Linux SocketCAN factories follow the same conventions as the other product classes.

## Device ID

OP3 uses only the standard protocol. The initialized instance provides `GetNonPrivateHandDeviceIdByBroadcast()`, `GetHandDeviceIdByBroadcast()`, and `SetHandDeviceIdByBroadcast()`. The broadcast ID is `0`, and the default unicast ID is `1`. OP3 has no private-protocol ID.

## Position and Status

```cpp
int16_t SetJointMotorPosi(unsigned char joint_motor_index, int16_t position);
JointPosiStatus SetJointMotorPosiWithStatus(
    uint8_t joint_motor_index, int16_t position);
JointDetailInfo GetJointDetailInfo(uint8_t joint_index) const;
```

`SetJointMotorPosiWithStatus()` uses a 1-based motor index. `GetJointDetailInfo()` uses a 0-based joint index. The detailed status calls are implemented by RS485; unsupported backends return an all-zero structure.

## Tactile Metadata

`ITactileSensor1DU16` provides sensor count, point count, datum size, returned-frame count, sensor serial number, sensor order, and raw tactile frames.

## Mixed Control

On OP3, the mixed-control bus protocol implements **position + current (`MixControlMode::POSITION_TORQUE`)** only. The current field is `int16` in mA; its range is `0..6500`, default `3000`.

`OmniHand` also declares `MixControlByPV` and `MixControlByPVT`. On OP3 these log an error and return an empty vector; **no CAN frame is sent**.

| API | Support |
|-----|---------|
| `MixControlByPT(positions, torques)` | Supported |
| `MixControlByPV(velocities, torques)` | **Not supported** |
| `MixControlByPVT(positions, velocities, torques)` | **Not supported** |

```cpp
#include "omnihand/omnipicker_3.h"

// Array index i maps to mix-control joint id i (0-based); OP3 has one joint.
std::vector<MixCtrl> echo = hand->MixControlByPT({pos}, {torque});

// Not available on OP3; returns {}
hand->MixControlByPV({vel}, {torque});
hand->MixControlByPVT({pos}, {vel}, {torque});
```

Echo `joint_index_` is **1-based** in the reply (consistent with O10/O12 mixed-control readback).

## Related Documentation

- [C++ API index](API_CPP.md)
- [1.1.8 to 1.1.9 migration](MIGRATION_1_1_8_TO_1_1_9.md)
