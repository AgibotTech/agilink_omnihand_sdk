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

Every explicit nonzero device ID, including `kT1P3DefaultHandDeviceId` (`9`), skips broadcast and address fallback but still queries DOF. The device address does not select a hardware revision: a valid 20- or 21-joint reply determines the implementation, and a failed or unsupported reply returns `nullptr`.

Automatic discovery requires one H3UM per selected CAN channel. On a shared bus, pass the target's explicit ID; this skips broadcast and legacy fallback. Use `GetNumOfJointMotors()` and `GetJointNames()` rather than hardcoding the joint count or order.

```cpp
static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0,
    bool show_data_details = false);

static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& usbcanfd_serial_number,
    uint8_t canfd_channel_id = 0,
    bool show_data_details = false);

static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    uint8_t canfd_device_id, uint8_t canfd_channel_id = 0,
    bool show_data_details = false);

static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
    HandType hand_type, uint8_t hand_device_id,
    const std::string& hcan_serial_number,
    uint8_t canfd_channel_id = 0,
    bool show_data_details = false);
```

Linux `createHandSocketCan` uses the same discovery flow. Pass `kBroadcastHandDeviceId` as the hand device ID to enable automatic discovery, including when supplying an adapter serial number or SocketCAN interface.

The final `show_data_details` argument defaults to `false`. To register a per-device log callback before connection, use `DeviceContext::SetLogCallback()` with `CreateHand(context)`.

T1.3 uses the default external tactile SoC endpoint `192.168.99.2:19009`. To change it after creation, call `AttachTactileSoc(ip, port)`. This waits for old tactile workers to stop and initializes the new endpoint; finger scanning is asynchronous. An empty IP detaches the service. It returns `true` on detach or a successful endpoint probe, and `false` for T1.5, an invalid nonempty endpoint, or a failed probe.

```cpp
bool AttachTactileSoc(
    const std::string& tactile_soc_ip,
    uint16_t tactile_soc_port = kDefaultTactileSocPort);
```

The dedicated `createHandT1P5ByZlgcan` factories have been removed; use `createHandByZlgcan` for automatic revision detection. `GetRevision()` remains available for diagnostics and offline model selection.

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
```

Torque is transmitted directly as a signed 16-bit current value in mA. Call `hand->GetMinMaxDefaultMixCtrlTorque(joint_motor_index)` on the constructed hand: T1.3 reports range `0..10000` and default `1000`; T1.5 reports range `0..2000` and default `1900`. Motor indices start at `1`; an index outside the hand's actual joint count returns `{0, 0, 0}`. This is now a `const` instance method; replace previous class-qualified static calls with calls through the hand object.

## Preset Gestures

Use `GetHandGesture(int)` to retrieve motor encoder values or `SetHandGesture(int)` to execute a gesture. Both also accept `h3um::H3UMGesture`.

| ID | Enum | Pose | Revision |
| --- | --- | --- | --- |
| 0 | `H3U_M_GESTURE_ALL_ZERO` | All encoder values zero | T1.3 / T1.5 |
| 1 | `H3U_M_GESTURE_FRONT_FIST` | Front fist | T1.3 / T1.5 |
| 2 | `H3U_M_GESTURE_SIDE_FIST` | Side fist | T1.3 / T1.5 |
| 3 | `H3U_M_GESTURE_FINGER_HEART` | Finger heart | T1.3 / T1.5 |
| 4 | `H3U_M_GESTURE_OPEN` | Open | T1.3 / T1.5 |

Presets store motor encoder values selected by the hand object's revision and side: T1.3 returns 20 axes and T1.5 returns 21. Both revisions share the same gesture IDs. When using a standalone Solver, supply the revision and hand side explicitly.

Switching to a different non-open pose (IDs 1–3, including switches from open) first retains the preceding pose's other joints and sets the four thumb ABAD, MCP, PIP, and DIP encoders to `0`, waits `200 ms`, then executes the target. These are axes 17–20 on T1.3 and 18–21 on T1.5 (1-based). T1.5 thumb ROLL at axis 17 retains its preceding value. A failed transition prevents the target command. First calls, repeated poses, and switches to open have no transition. ID 0 remains available for compatibility and does not participate in transitions.

## Factory Maintenance

Factory calibration, zeroing and serial-number writes are available only through OmniHand-ToolBox. These maintenance operations are not part of the public SDK API.

## Tactile Data and T1.5 Board Temperatures

### T1.3: Xense Tactile SoC

T1.3 uses the following Xense tactile SoC APIs:

```cpp
bool GetFingerTactile(uint8_t finger_index, XenseFrame& frame); // 0=thumb .. 4=pinky
bool GetPalmTactile(PalmFrame& frame);
bool GetAllTactile(AllTactileFrame& frame);
```

`XenseFrame::timestamp` is in nanoseconds. `PalmFrame` contains the force field, `resultant[3]`, and `timestamp` supplied by the tactile service.

### T1.3: Downsampled 3D Tactile Force and Resultant Force

T1.3 provides downsampled 3D tactile force and resultant force for six sensors over CAN. The following methods belong to `ITactileSensor3DI16`, obtained through `hand->GetTactileSensor3D()`:

```cpp
TactileSensorData3DI16 GetTactileSensorDataRaw(Finger sensor) const;
std::vector<TactileSensorData3DI16> GetAllTactileSensorDataRaw() const;
TactileSensorResultantForce3DI16 GetTactileSensorResultantForce(Finger sensor) const;
std::vector<TactileSensorResultantForce3DI16> GetAllTactileSensorResultantForces() const;
```

```cpp
if (auto* tactile = hand->GetTactileSensor3D()) {
  auto data = tactile->GetTactileSensorDataRaw(Finger::THUMB);
  auto forces = tactile->GetAllTactileSensorResultantForces();
}
```

`TactileSensorData3DI16::points` contains signed 16-bit XYZ components for each downsampled point. Resultant force is retrieved through the separate resultant-force methods.

### T1.5: 1D U16 Tactile Data

T1.5 uses the separate `ITactileSensor1DU16` interface, obtained through `hand->GetTactileSensor1D()`:

```cpp
TactileSensorDataU16 GetTactileSensorData(Finger sensor) const;
TactileSensorDataU16 GetTactileSensorDataRaw(Finger sensor) const;
std::vector<TactileSensorDataU16> GetAllTactileSensorDataRaw() const;
```

```cpp
if (auto* tactile = hand->GetTactileSensor1D()) {
  auto data = tactile->GetTactileSensorDataRaw(Finger::THUMB);
  auto all_data = tactile->GetAllTactileSensorDataRaw();
}
```

`TactileSensorDataU16::data_` contains one `uint16_t` pressure value per point. T1.5 does not expose the T1.3 3D tactile-force or resultant-force interfaces above. Both interface accessors return borrowed pointers valid for the hand object's lifetime; an unsupported dimension returns `nullptr`.

### T1.5: Board Temperatures

Call these methods directly on the hand object:

```cpp
int16_t GetTemperatureOfPalm() const;
int16_t GetTemperatureOfPower() const;
```

Board temperatures are supported only on T1.5. On T1.3, both getters and their timestamped variants return a temperature value of `0` without sending a CAN request.

## C++ Demo

All transports share [demo_omnihand_3_ultra_m.cc](../../../cpp/demo/omnihand_3_ultra_m/demo_omnihand_3_ultra_m.cc), built as `demo_omnihand_3_ultra_m`:

```shell
./demo_omnihand_3_ultra_m -d zlgcan -i 0 -c 0
./demo_omnihand_3_ultra_m -d hcan --serial ADAPTER_SERIAL
./demo_omnihand_3_ultra_m -d socketcan --can-if can0
./demo_omnihand_3_ultra_m -d zlgcan -c 0 --hand both
./demo_omnihand_3_ultra_m -d zlgcan --xense
```

Windows executables use the `.exe` suffix; Linux serial ports can use `/dev/ttyUSB0`. SocketCAN is Linux-only. Use `--help` for all options. Demos cover normal control and information queries; they do not update firmware, calibrate, set zero offsets, write serial numbers, or write factory parameters.

`--hand both` uses `-c` and the next channel on one CAN adapter. With `--serial` and `--second-serial`, both adapters use the selected channel number.

The demo defaults to `--device-id 0` to discover the address and detect T1.3/T1.5 automatically; there is no `--revision` option. Each selected CAN channel must contain one hand for discovery; use an explicit `--device-id` on a shared bus. `--xense` requires an SDK built with Xense support and reads visual tactile data instead of running normal motion.

The demo reads the current pose, moves only the pinky PIP joint to 0.35 and 0.7 rad, and reads feedback after each move. The same flow supports T1.3 and T1.5. It then reads motor temperatures, currents and errors; T1.3 reads 3D tactile points and resultant forces, while T1.5 reads U16 pressure points and palm/power board temperatures.

See [CMakeLists.txt](../../../cpp/demo/omnihand_3_ultra_m/CMakeLists.txt) for build instructions.

## Related Documentation

- [C++ API index](API_CPP.md)
- [1.1.8 to 1.1.9 migration](MIGRATION_1_1_8_TO_1_1_9.md)
- [H3UM ROS2 interface](API_ROS2_H3UM.md)
