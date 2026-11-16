# OmniHand 3 Ultra M（H3UM）C++ API

## 概述

`OmniHand3UltraM` 使用统一工厂函数连接所支持的 H3UM 硬件。SDK 自动发现设备地址，读取主动关节数，再选择协议解析、运动学标定和触觉接口。电机位置使用 `[0, 4096]` 协议值，主动关节角单位为弧度。

## 包含头文件

```cpp
#include "omnihand/omnihand_3_ultra_m.h"
using namespace agilink::omnihand;
```

## 工厂函数

```cpp
auto hand = OmniHand3UltraM::createHandByZlgcan(
    HandType::LEFT, kBroadcastHandDeviceId, 0);
if (hand && hand->Init()) {
  const auto joint_count = hand->GetNumOfJointMotors();
  const auto joint_names = hand->GetJointNames();
}
```

`hand_device_id` 显式传入 `kBroadcastHandDeviceId`（`0`）时启用自动发现：SDK 广播读取设备信息（`0x02/0x00`），仅在无回复时探测旧设备地址 `9`，再单独读取自由度寄存器（`0x01/0x06`）。收到有效的 20 或 21 关节回复后选择对应实现；读取失败或自由度不受支持时返回 `nullptr`。

显式传入任意非零设备 ID（包括 `kT1P3DefaultHandDeviceId`，即 `9`）时，跳过广播和地址回退，但仍读取自由度。设备地址不决定硬件版本：有效的 20 或 21 关节回复决定对应实现，读取失败或自由度不受支持时返回 `nullptr`。

自动发现适用于所选 CAN 通道只有一台 H3UM 的场景。同一总线上有多台设备时，请显式传入目标设备 ID，此时跳过广播和旧地址回退。关节数和顺序应通过 `GetNumOfJointMotors()` 与 `GetJointNames()` 获取。

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

`createHandByZlgCanTcp` 和 Linux 下的 `createHandSocketCan` 使用相同的自动识别流程。按适配器序列号、TCP 端点或 SocketCAN 接口创建时，设备 ID 显式传入 `kBroadcastHandDeviceId` 即可启用自动发现。

末尾的 `show_data_details` 默认值为 `false`。需要在连接前注册设备日志回调时，通过 `DeviceContext::SetLogCallback()` 配合 `CreateHand(context)` 使用。

T1.3 默认使用外部触觉 SoC 地址 `192.168.99.2:19009`。创建后调用 `AttachTactileSoc(ip, port)` 可更换地址：先等待旧触觉线程退出，再初始化新地址，五指扫描异步执行。IP 为空时断开该服务。断开成功或端点探测成功返回 `true`；T1.5、不合法的非空端点或探测失败返回 `false`。

```cpp
bool AttachTactileSoc(
    const std::string& tactile_soc_ip,
    uint16_t tactile_soc_port = kDefaultTactileSocPort);
```

已移除专用的 `createHandT1P5ByZlgcan` 创建接口，请使用 `createHandByZlgcan` 自动识别版本。`GetRevision()` 仍可用于诊断和离线模型选择。

## 设备信息与地址

```cpp
VendorInfo GetVendorInfo() const;
std::string GetSN() const;
DeviceInfo GetDeviceInfo() const;

uint8_t GetNonPrivateHandDeviceIdByBroadcast();
int GetHandDeviceIdByBroadcast();
bool SetHandDeviceIdByBroadcast(uint8_t id);
void SetBroadcast(bool broadcast);
```

H3UM 只有标准协议。设备 ID `0` 为广播地址；T1.3 默认单播 ID 为 `9`，T1.5 为 `1`。

## 位置与角度控制

```cpp
int16_t SetJointMotorPosi(unsigned char joint_motor_index, int16_t position);
int16_t GetJointMotorPosi(unsigned char joint_motor_index) const;
std::vector<int16_t> SetAllJointMotorPosi(const std::vector<int16_t>& positions);
std::vector<int16_t> GetAllJointMotorPosi() const;

std::vector<double> SetAllActiveJointAngles(const std::vector<double>& angles);
std::vector<double> GetAllActiveJointAngles() const;
```

手支持 CSP（`ControlMode::POSITION`，值为 `0`）和 PP（`ControlMode::PROFILE_POSITION`，值为 `7`）。CSP/PP 与“仅位置”或“位置+力矩”的控制类型相互独立。

### CSP 连续控制

```cpp
bool StartCsp(double max_velocity_ticks_per_second);
bool SetCspTarget(const std::vector<int16_t>& positions);
void StopCsp();
bool IsCspRunning() const;
```

`StartCsp` 使用当前位置启动固定 500 Hz 后台发送，将请求间隔设为 `2 ms`，并按 `max_velocity_ticks_per_second` 限制位置变化。该参数是 motor tick/s，不是发送频率。调用者只需在目标变化时调用 `SetCspTarget`。`StopCsp` 恢复启动前的控制模式和请求间隔。

```cpp
if (hand->StartCsp(1000.0)) {
  hand->SetCspTarget(target_positions);
  // ...持续更新目标...
  hand->StopCsp();
}
```

### CSP 动作序列

```cpp
struct CspSequence {
  std::vector<uint32_t> timestamps_ms;
  // 按行展开：point_count * joint_count。
  std::vector<int16_t> positions;
};

bool PlaySequence(CspSequence&& sequence);
void StopSequence();
bool IsSequencePlaying() const;
```

当前位置是 `0 ms` 的隐式起点，SDK按时间戳线性插值并以500 Hz发送。播放结束后持续保持最后一点，直到调用 `StopSequence()`。

```cpp
CspSequence sequence;
sequence.timestamps_ms = {500, 1200};
sequence.positions.reserve(2 * pose_a.size());
sequence.positions.insert(sequence.positions.end(), pose_a.begin(), pose_a.end());
sequence.positions.insert(sequence.positions.end(), pose_b.begin(), pose_b.end());
hand->PlaySequence(std::move(sequence));
```

两块连续数组避免大轨迹为每个点单独分配内存。通过 `std::move` 将数据交给异步播放器，不产生额外复制。如果显式提供 `0 ms` 点，其位置必须与设备当前位置一致。

已有动作容器（例如 toolbox 的 `std::vector<SeqStep>`）可直接通过数据源播放，无需转成连续数组：

```cpp
bool PlaySequence(CspSequenceSource source);
```

数据源持有原容器的 `shared_ptr` 和静态读取函数。读取函数将第 `index` 步的位置写入 SDK 提供的缓冲区，并返回从上一目标到本步目标的 `duration_ms`；第 0 步从设备当前位置开始。除第 0 步可为 `0 ms` 外，持续时间必须大于 0。

以下读取函数可定义为 toolbox `ControlPanelBackend` 的静态成员；`SeqStep` 是已有类型：

```cpp
static bool ReadCspPoint(const void* data, size_t index,
                         uint32_t& duration_ms, int16_t* positions,
                         size_t joint_count) {
  const auto& steps = *static_cast<const std::vector<SeqStep>*>(data);
  const auto& step = steps.at(index);
  if (step.positions.size() != joint_count || step.delay_ms <= 0) {
    return false;
  }
  duration_ms = static_cast<uint32_t>(step.delay_ms);
  for (size_t j = 0; j < joint_count; ++j) {
    positions[j] = static_cast<int16_t>(step.positions[j]);
  }
  return true;
}

// stepsCopy 是已有播放数据的 shared_ptr，无需再打包关节数组。
CspSequenceSource source{stepsCopy, stepsCopy->size(), &ReadCspPoint};
bool started = hand->PlaySequence(std::move(source));
```

SDK 在启动时校验数据，播放时只缓存相邻两步。读取函数应只读内存；播放期间原数据保持不变，未解析的动作/手势需提前解析，`none` 步骤需由上层明确处理。SDK 持有数据所有者直到 `StopSequence()`，停止后释放自己的引用。

## 运动学求解器

`agilink::omnihand::h3um::OmniHand3UltraMSolver` 用于主动关节角与电机位置的离线换算，无需连接设备。

```cpp
#include "omnihand/kinematics/omnihand_3_ultra_m/omnihand_3_ultra_m_solver.h"

using namespace agilink::omnihand;
using namespace agilink::omnihand::h3um;

OmniHand3UltraMSolver solver(true, H3UMRevision::T1P5);  // Left T1.5; use T1P3 for T1.3.
std::vector<int16_t> motor_positions(solver.GetActiveJointCount(), 0);
auto angles = solver.ActuatorInput2ActiveJointPos(motor_positions);
auto actuator = solver.ActiveJointPos2ActuatorInput(angles);
```

单参数构造 `OmniHand3UltraMSolver(is_left_hand)` 默认使用 T1.3。T1.3 向量必须包含 20 个值，T1.5 必须包含 21 个值，顺序与对应版本设备的 `GetJointNames()` 一致。角度单位为弧度，电机输入使用 `[0, 4096]` 协议值。向量长度不正确时，换算返回空向量。

单关节换算使用从 0 开始的索引（T1.3 为 `0..19`，T1.5 为 `0..20`）：

```cpp
int16_t SingleActiveJointPos2ActuatorInput(std::size_t joint_index, double rad) const;
double SingleActuatorInput2ActiveJointPos(std::size_t joint_index, int16_t actuator_input) const;
```

以上方法仅进行数值计算。需要将换算结果下发到设备时，请另外调用 `SetAllJointMotorPosi`。

## 混合控制

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

力矩按有符号 16 位电流值直接下发，单位为 mA。通过构造后的对象调用 `hand->GetMinMaxDefaultMixCtrlTorque(joint_motor_index)`：T1.3 返回范围 `0..10000`、默认值 `1000`；T1.5 返回范围 `0..2000`、默认值 `1900`。电机索引从 `1` 开始，超出实际关节数时返回 `{0, 0, 0}`。该接口现为 `const` 成员函数，原先通过类名调用的静态写法需改为通过手对象调用。

## 预设手势

通过 `GetHandGesture(int)` 获取电机编码，或调用 `SetHandGesture(int)` 执行手势；也可使用 `h3um::H3UMGesture` 枚举重载。

| ID | 枚举 | 姿态 | 版本 |
| --- | --- | --- | --- |
| 0 | `H3U_M_GESTURE_ALL_ZERO` | 电机编码全零 | T1.3 / T1.5 |
| 1 | `H3U_M_GESTURE_FRONT_FIST` | 正握拳 | T1.3 / T1.5 |
| 2 | `H3U_M_GESTURE_SIDE_FIST` | 侧握拳 | T1.3 / T1.5 |
| 3 | `H3U_M_GESTURE_FINGER_HEART` | 比心 | T1.3 / T1.5 |
| 4 | `H3U_M_GESTURE_OPEN` | 打开 | T1.3 / T1.5 |

预设以电机编码保存，由手对象的版本及左右手选择对应数据：T1.3 返回 20 轴，T1.5 返回 21 轴。两版复用相同手势 ID，无需调用者区分；独立使用 Solver 时需传入版本和左右手。

切换到不同的非打开姿态（ID 1–3，包括从打开姿态切换）时，SDK 先保持上一姿态的其他关节，将拇指 ABAD、MCP、PIP、DIP 四轴电机编码设为 `0`，等待 `200 ms` 后再执行目标姿态：T1.3 对应第 17–20 轴，T1.5 对应第 18–21 轴（从 1 开始计数）。T1.5 的第 17 轴拇指 ROLL 保持上一姿态。过渡失败则不发送目标；首次执行、重复同一姿态及切换到打开姿态不加过渡。ID 0 保留兼容，不参与过渡。

## 出厂维护

出厂标定、标零、写 SN 等维护操作仅供 OmniHand-ToolBox 使用，不属于公开 SDK API。

## 触觉数据与 T1.5 板温

### T1.3：Xense 触觉 SoC

T1.3 使用以下 Xense 触觉 SoC 接口：

```cpp
bool GetFingerTactile(uint8_t finger_index, XenseFrame& frame); // 0=拇指 .. 4=小指
bool GetPalmTactile(PalmFrame& frame);
bool GetAllTactile(AllTactileFrame& frame);
```

`XenseFrame::timestamp` 单位为纳秒。`PalmFrame` 包含力场、`resultant[3]` 和触觉服务提供的 `timestamp`。

### T1.3：3D 触觉力降采样与合力

T1.3 通过 CAN 获取六个传感器的降采样 3D 触觉力和合力。以下方法属于 `ITactileSensor3DI16`，通过 `hand->GetTactileSensor3D()` 获取该接口：

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

`TactileSensorData3DI16::points` 保存降采样后各点的有符号 16 位 XYZ 分量；合力通过独立的合力接口获取。

### T1.5：1D U16 触觉

T1.5 使用独立的 `ITactileSensor1DU16` 接口，通过 `hand->GetTactileSensor1D()` 获取：

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

`TactileSensorDataU16::data_` 保存每点的 `uint16_t` 压力值。T1.5 不提供上述 T1.3 的 3D 触觉力及合力接口。两个触觉接口获取函数返回的指针均随手对象存活；不支持的维度返回 `nullptr`。

### T1.5：板温

以下方法直接通过手对象调用：

```cpp
int16_t GetTemperatureOfPalm() const;
int16_t GetTemperatureOfPower() const;
```

板温接口仅支持 T1.5。T1.3 调用上述两个接口及其带时间戳版本时，温度值直接返回 `0`，不发送 CAN 请求。

## 相关文档

- [C++ API 总览](API_CPP.md)
- [从 1.1.8 迁移到 1.1.9](MIGRATION_1_1_8_TO_1_1_9.md)
- [H3UM ROS2 接口](API_ROS2_H3UM.md)
