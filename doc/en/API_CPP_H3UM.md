# OmniHand 3 Ultra M (H3UM) C++ API

## Overview

`OmniHand3UltraM` uses one set of factories for supported H3UM hardware. The SDK discovers the device address and reads its active-joint count before selecting the protocol, kinematics calibration, and tactile interface. Motor positions use `[0, 4096]` protocol units and active joint angles are radians.

## Include Header

```cpp
#include "omnihand/omnihand_3_ultra_m.h"
using namespace agilink::omnihand;
```

## Factory Methods

```cpp
auto hand = OmniHand3UltraM::createHandByZlgcan(
    HandType::LEFT, kBroadcastHandDeviceId, 0);
if (hand && hand->Init()) {
  const auto joint_count = hand->GetNumOfJointMotors();
  const auto joint_names = hand->GetJointNames();
}
```

Pass `kBroadcastHandDeviceId` (`0`) as `hand_device_id` to enable automatic discovery: the SDK broadcasts a device-information read (`0x02/0x00`), falls back to legacy address `9` only if no reply arrives, then reads the standalone DOF register (`0x01/0x06`). A valid 20- or 21-joint reply selects the implementation. Failed or unsupported replies return `nullptr`.

Explicitly passing `kT1P3DefaultHandDeviceId` (`9`) selects T1.3 directly without broadcast or DOF probing. Other explicit IDs still query DOF. An address discovered automatically, including `9`, is always verified by a DOF read.

Automatic discovery requires one H3UM per selected CAN channel. On a shared bus, pass the target's explicit ID; this skips broadcast and legacy fallback. Use `GetNumOfJointMotors()` and `GetJointNames()` rather than hardcoding the joint count or order.

```cpp
static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort,
    CommunicationLogOptions log_options = {});

static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& usbcanfd_serial_number,
    uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort,
    CommunicationLogOptions log_options = {});

static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort,
    CommunicationLogOptions log_options = {});

static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& hcan_serial_number,
    uint8_t canfd_channel_id = 0,
    const std::string& tactile_soc_ip = kDefaultTactileSocIp,
    uint16_t tactile_soc_port = kDefaultTactileSocPort,
    CommunicationLogOptions log_options = {});
```

`createHandByZlgCanTcp` and Linux `createHandSocketCan` use the same discovery flow. Pass `kBroadcastHandDeviceId` as the hand device ID to enable automatic discovery, including when supplying an adapter serial number, TCP endpoint, or SocketCAN interface.

The optional tactile SoC endpoint defaults to `192.168.99.2:19009`. It is used only by hardware with an external Xense/palm service, and only initialized when reachable. An empty `tactile_soc_ip` disables that service; hardware with CAN-based tactile data ignores these endpoint parameters.

The existing `createHandT1P5*` factories remain available for source compatibility and retain their explicit hardware selection. New applications should use the unified factories above. `GetRevision()` remains available for diagnostics and offline model selection.

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

## Kinematics Solver

`agilink::omnihand::h3um::OmniHand3UltraMSolver` converts between active joint angles and motor positions without connecting to a device.

```cpp
#include "omnihand/kinematics/omnihand_3_ultra_m/omnihand_3_ultra_m_solver.h"

using namespace agilink::omnihand;
using namespace agilink::omnihand::h3um;

OmniHand3UltraMSolver solver(true, H3UMRevision::T1P5);  // Left T1.5; use T1P3 for T1.3.
std::vector<int16_t> motor_positions(solver.GetActiveJointCount(), 0);
auto angles = solver.ActuatorInput2ActiveJointPos(motor_positions);
auto actuator = solver.ActiveJointPos2ActuatorInput(angles);
```

The one-argument constructor `OmniHand3UltraMSolver(is_left_hand)` defaults to T1.3. Vectors must contain 20 values for T1.3 or 21 for T1.5, in the same order as the matching device's `GetJointNames()`. Angles are radians; motor inputs use `[0, 4096]` protocol units. Vector conversions return an empty vector if the input length is wrong.

Single-joint conversions use a zero-based index (`0..19` for T1.3, `0..20` for T1.5):

```cpp
int16_t SingleActiveJointPos2ActuatorInput(std::size_t joint_index, double rad) const;
double SingleActuatorInput2ActiveJointPos(std::size_t joint_index, int16_t actuator_input) const;
```

These methods only calculate values. To send the converted positions to a device, call `SetAllJointMotorPosi` separately.

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
