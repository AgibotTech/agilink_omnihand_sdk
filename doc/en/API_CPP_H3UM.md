# OmniHand 3 Ultra M (H3UM T1.3/T1.5) C++ API

## Overview

`OmniHand3UltraM` controls both H3UM revisions through CAN FD. T1.3 has 20 active joints and uses the Xense tactile SoC; T1.5 has 21 active joints and obtains tactile data directly through CAN FD. Motor positions use `[0, 4096]` protocol units and active joint angles are radians.

| Revision | Active joints | Default device ID | Factory prefix |
|----------|---------------|-------------------|----------------|
| T1.3 | 20 | 9 | `createHandBy*` / `createHandSocketCan` |
| T1.5 | 21 | 1 | `createHandT1P5By*` / `createHandT1P5SocketCan` |

## Include Header

```cpp
#include "omnihand/omnihand_3_ultra_m.h"
using namespace agilink::omnihand;
```

## Factory Methods

T1.3 factories accept an optional tactile SoC endpoint after the transport parameters:

```cpp
const std::string& tactile_soc_ip = OmniHand3UltraM::kDefaultTactileSocIp;
uint16_t tactile_soc_port = OmniHand3UltraM::kDefaultTactileSocPort;
```

The defaults are `192.168.99.2:19009`. The SDK probes the endpoint during construction and initializes Xense and palm services only when it is reachable. Applications using the standard endpoint should omit both arguments.

```cpp
static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort);

static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& usbcanfd_serial_number,
    uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort);

static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort);

static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& hcan_serial_number,
    uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort);
```

`createHandByZlgCanTcp(...)` and Linux `createHandSocketCan(...)` have the same final two tactile endpoint parameters.

T1.5 uses separate factories and does not take a tactile SoC endpoint:

```cpp
createHandT1P5ByZlgcan(...);
createHandT1P5ByZlgCanTcp(...);
createHandT1P5ByHcan(...);
createHandT1P5SocketCan(...);  // Linux
```

The selected revision is available at runtime:

```cpp
H3UMRevision GetRevision() const;
std::vector<std::string> GetJointNames() const;
uint8_t GetNumOfJointMotors() const;
```

## Device Information and Addressing

```cpp
VendorInfo GetVendorInfo() const;
std::string GetSN() const;
DeviceInfo GetDeviceInfo() const;

uint8_t GetNonPrivateHandDeviceIdByBroadcast();
int GetHandDeviceIdByBroadcast();
bool SetHandDeviceIdByBroadcast(uint8_t id);
void SetBroadcast(bool broadcast);
```

H3UM uses only the standard protocol. Device ID `0` is its broadcast address. The default unicast ID is `9` for T1.3 and `1` for T1.5.

## Position and Angle Control

```cpp
int16_t SetJointMotorPosi(unsigned char joint_motor_index, int16_t position);
int16_t GetJointMotorPosi(unsigned char joint_motor_index) const;
std::vector<int16_t> SetAllJointMotorPosi(const std::vector<int16_t>& positions);
std::vector<int16_t> GetAllJointMotorPosi() const;

std::vector<double> SetAllActiveJointAngles(const std::vector<double>& angles);
std::vector<double> GetAllActiveJointAngles() const;
```

The hand supports CSP (`ControlMode::POSITION`, value `0`) and PP (`ControlMode::PROFILE_POSITION`, value `7`). Control mode is independent of whether position-only or position+torque commands are used.

## Mixed Control

```cpp
MixCtrl MixControlByPT(uint8_t joint_motor_index,
                       int16_t position, int16_t torque);
std::vector<MixCtrl> MixControlByPT(
    const std::vector<int16_t>& positions,
    const std::vector<int16_t>& torques);

MixCtrl MixControlByPVT(uint8_t joint_motor_index,
                        int16_t position, int16_t velocity,
                        int16_t torque);
std::vector<MixCtrl> MixControlByPVT(
    const std::vector<int16_t>& positions,
    const std::vector<int16_t>& velocities,
    const std::vector<int16_t>& torques);
```

Torque is transmitted directly as a signed 16-bit current value in mA. `GetMinMaxDefaultMixCtrlTorque()` reports range `0..10000` and default `1000`.

## Zero and Calibration

```cpp
bool SetAllMotor2Zero();
bool SetMotor2Zero(unsigned char joint_motor_index);  // revision-dependent range
bool SetAxisHoming(uint8_t axis_index, int16_t position);
bool SetAxisMinPos(uint8_t axis_index, int16_t min_position);
bool SetAxisMaxPos(uint8_t axis_index, int16_t max_position);
bool SaveParam();
```

## Tactile Data and T1.5 Board Temperatures

T1.3 uses the Xense tactile SoC APIs:

```cpp
bool GetFingerTactile(uint8_t finger_index, XenseFrame& frame); // 0=thumb .. 4=pinky
bool GetPalmTactile(PalmFrame& frame);
bool GetAllTactile(AllTactileFrame& frame);
```

`XenseFrame::timestamp` is in nanoseconds. `PalmFrame` contains the force field, `resultant[3]`, and `timestamp` supplied by the tactile service.

T1.5 uses the common raw tactile APIs and supplies resultant force for six sensors:

```cpp
TactileSensorData3DI16 GetTactileSensorDataRaw(Finger sensor) const;
std::vector<TactileSensorData3DI16> GetAllTactileSensorDataRaw() const;
TactileSensorResultantForce3DI16 GetTactileSensorResultantForce(Finger sensor) const;
std::vector<TactileSensorResultantForce3DI16> GetAllTactileSensorResultantForces() const;
int16_t GetTemperatureOfPalm() const;
int16_t GetTemperatureOfPower() const;
```

## Related Documentation

- [C++ API index](API_CPP.md)
- [1.1.8 to 1.1.9 migration](MIGRATION_1_1_8_TO_1_1_9.md)
- [H3UM ROS2 interface](API_ROS2_H3UM.md)
