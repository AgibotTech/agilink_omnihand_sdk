# OmniHandPicker 2025 (OP1) C++ API

[中文](../zh_cn/API_CPP_OP1.md) | [C++ API Index](API_CPP.md)

OP1 is a 1-DOF gripper. Its C++ interface is `agilink::omnihand::OmniPicker2025`, declared in `omnihand/omnipicker_2025.h`. No Python binding is currently provided.

## Connection

| Connection | Factory method | Platform |
|------------|----------------|----------|
| ZLG USB CANFD | `createHandByZlgcan(hand_type, hand_device_id, canfd_device_id, canfd_channel_id)` | Windows / Linux |
| HCAN USB CANFD | `createHandByHcan(hand_type, hand_device_id, canfd_device_id, canfd_channel_id)` | Windows / Linux |
| SocketCAN | `createHandSocketCan(hand_type, hand_device_id, "can0")` | Linux |
| ZLG CANFD over TCP | `createHandByZlgCanTcp(hand_type, hand_device_id, tcp_host, tcp_port, canfd_channel_id)` | Windows / Linux x64 |
| USB direct | `createHandByUSB()` | Windows / Linux |

For CAN connections, pass the gripper's actual node ID; the default is `OmniPicker2025::kDefaultHandDeviceId` (1). Adapter and channel indices start at 0. `hand_type` is retained in the factory signature; the example uses `HandType::LEFT`. ZLG and HCAN factories also accept an adapter serial number instead of its index.

USB direct uses the Fibre USB protocol and does not take a COM port. RS485 is not supported. See [Quick Start](QUICK_START.md) for installation and wiring, or [SocketCAN Setup](SOCKETCAN_SETUP.md) for Linux CAN configuration.

## CANFD Control and Feedback

The following example sends an opening command at the lowest speed and force settings, then checks the returned state. Adjust speed and force within the reported ranges for your application.

```cpp
#include <iostream>
#include "omnihand/omnipicker_2025.h"

using namespace agilink::omnihand;

int main() {
  auto picker = OmniPicker2025::createHandByZlgcan(
      HandType::LEFT, OmniPicker2025::kDefaultHandDeviceId, 0, 0);
  if (!picker || !picker->Init()) {
    std::cerr << "Failed to initialize CAN adapter\n";
    return 1;
  }

  const auto range = picker->GetCanfdFrameRange();
  const Op1CanfdCtrlFrame command{
      range.pos.max_value,
      range.vel.min_value,
      range.tor.min_value,
      range.acc.default_value,
      range.dec.default_value};
  const auto feedback = picker->SendFrameSyncByCanfd(command);
  if (!feedback) {
    std::cerr << "No valid OP1 response\n";
    return 1;
  }
  std::cout << ToString(*feedback) << '\n';
  if (feedback->fault_code != static_cast<uint8_t>(Op1FaultCode::NO_FAULT)) {
    return 1;
  }
  return 0;
}
```

`Init()` checks adapter initialization; it does not confirm that the gripper responds. `SendFrameSyncByCanfd()` returns `std::optional<Op1CanfdStateFrame>`; timeout, communication failure, or an invalid response length produces `std::nullopt`.

### Command Fields

| `Op1CanfdCtrlFrame` field | Range | Meaning |
|-------------------------|-------|---------|
| `pos_cmd` | 0–255 | 0 = fully closed, 255 = fully open |
| `vel_cmd` | 1–255 | Speed setting |
| `force_cmd` | 1–255 | Grip force setting; range is `GetCanfdFrameRange().tor` |
| `acc_cmd` | 1–255 | Acceleration setting |
| `dec_cmd` | 1–255 | Deceleration setting |

These are protocol byte values, not millimetres, radians, or physical force units. `GetCanfdFrameRange()` supplies the ranges; the default speed, force, acceleration, and deceleration settings are all 255. Acceleration and deceleration must not be 0.

### Feedback Fields

`Op1CanfdStateFrame` contains `fault_code`, `state`, `pos`, `vel`, and `force`. Position, velocity, and force are raw feedback bytes. `ToString(*feedback)` formats the complete state for display.

| `Op1State` | Meaning |
|------------|---------|
| `ARRIVERED` | Target reached |
| `MOVING` | Moving toward target |
| `BLOCKED` | Motion blocked by contact or obstruction |
| `FALLED` | Motion failed |

The enum names above match the SDK spelling. A response does not mean motion is complete: check `state` for `Op1State::ARRIVERED`. To refresh feedback during movement, send the same target command again.

`Op1FaultCode::NO_FAULT` indicates no fault. Other codes are `OVER_TEMPTURE`, `OVER_SPEED`, `INIT_ERROR`, and `OVER_LIMIT`; use `ToString(static_cast<Op1FaultCode>(feedback->fault_code))` for the label.

## Preset Opening Positions

Both CAN and USB support the typed `OmniPicker2025Gesture` API:

| Gesture | Target opening ratio |
|---------|----------------------|
| `OMNIPICKER_2025_GESTURE_ZERO` | 0.0, fully closed |
| `OMNIPICKER_2025_GESTURE_HALF_OPEN` | 0.5, half open |
| `OMNIPICKER_2025_GESTURE_FULL_OPEN` | 1.0, fully open |

```cpp
const auto gesture = OmniPicker2025Gesture::OMNIPICKER_2025_GESTURE_FULL_OPEN;
const float target_ratio = picker->GetHandGesture(gesture);
const float result = picker->SetHandGesture(gesture);
```

`GetHandGesture()` returns the preset target ratio without querying the device. On CAN, `SetHandGesture()` returns the feedback opening ratio or -1 on failure; on USB it returns the requested ratio, not measured feedback. Neither return value confirms arrival.

CAN preset gestures use speed, force, acceleration, and deceleration settings of 255. Use `SendFrameSyncByCanfd()` when these settings need to be controlled.

## USB Direct

```cpp
#include <iostream>
#include "omnihand/omnipicker_2025.h"

using namespace agilink::omnihand;

int main() {
  auto picker = OmniPicker2025::createHandByUSB();
  if (!picker || !picker->Init()) {
    std::cerr << "Failed to initialize USB connection\n";
    return 1;
  }
  picker->SetHandGesture(
      OmniPicker2025Gesture::OMNIPICKER_2025_GESTURE_FULL_OPEN);
  std::cout << ToString(picker->ShowDeviceInfo()) << '\n';
  std::cout << "Press Enter to disconnect USB\n";
  std::cin.get();
  return 0;
}
```

`SendFrameSyncByCanfd()` and `GetCanfdFrameRange()` are CAN-only and throw `std::logic_error` on a USB instance. The current SDK reboots the device when the USB instance is destroyed; keep the instance alive while using the device. The example's return value does not indicate that opening has completed.

`ShowDeviceInfo()` reads device information: CAN fills device type and firmware version only; USB provides additional device information. Unqueried CAN fields keep their default values.
