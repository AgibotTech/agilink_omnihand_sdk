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

### Continuous CSP Control

```cpp
bool StartCsp(double max_velocity_ticks_per_second);
bool SetCspTarget(const std::vector<int16_t>& positions);
void StopCsp();
bool IsCspRunning() const;
```

`StartCsp` starts a fixed 500 Hz background stream from the current position, sets the request interval to `2 ms`, and limits motion by `max_velocity_ticks_per_second`. This parameter is measured in motor ticks per second; it is not the send frequency. Callers only update changed targets through `SetCspTarget`. `StopCsp` restores the previous control modes and request interval.

```cpp
if (hand->StartCsp(1000.0)) {
  hand->SetCspTarget(target_positions);
  // ...update the target as needed...
  hand->StopCsp();
}
```

### CSP Sequence Playback

```cpp
struct CspSequence {
  std::vector<uint32_t> timestamps_ms;
  // Row-major: point_count * joint_count.
  std::vector<int16_t> positions;
};

bool PlaySequence(CspSequence&& sequence);
void StopSequence();
bool IsSequencePlaying() const;
```

The current position is the implicit point at `0 ms`. The SDK linearly interpolates timestamped points and sends them at 500 Hz. After playback, it keeps sending the final point until `StopSequence()` is called.

```cpp
CspSequence sequence;
sequence.timestamps_ms = {500, 1200};
sequence.positions.reserve(2 * pose_a.size());
sequence.positions.insert(sequence.positions.end(), pose_a.begin(), pose_a.end());
sequence.positions.insert(sequence.positions.end(), pose_b.begin(), pose_b.end());
hand->PlaySequence(std::move(sequence));
```

The two contiguous arrays avoid one allocation per point for large trajectories. Pass the sequence with `std::move` to transfer its storage to the asynchronous player without copying. An explicit point at `0 ms` must match the current device position.

An existing action container can be used directly through a source:

```cpp
bool PlaySequence(CspSequenceSource source);

struct CspSequenceSource {
  using ReadPoint = bool (*)(const void*, size_t index,
                            uint32_t& duration_ms, int16_t* positions,
                            size_t joint_count);
  std::shared_ptr<const void> data;
  size_t point_count;
  ReadPoint read_point;
};

CspSequenceSource source{existing_steps, existing_steps->size(), &ReadCspPoint};
bool started = hand->PlaySequence(std::move(source));
```

The static reader writes one point into the SDK-provided buffer and returns the duration from the previous target to this target. Point 0 starts from the current device position. Durations must be positive, except point 0 may have zero duration if it matches the current position. The SDK validates points before starting and caches only two adjacent points during playback. The reader must access memory without blocking, and the source data must remain unchanged. Resolve action/gesture references and handle non-position steps before playback. The SDK retains the shared owner until `StopSequence()` releases its reference.

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
