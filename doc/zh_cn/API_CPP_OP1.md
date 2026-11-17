# OmniHandPicker 2025 (OP1) C++ API

[English](../en/API_CPP_OP1.md) | [C++ API 索引](API_CPP.md)

OP1 是 1 自由度夹爪，C++ 接口类为 `agilink::omnihand::OmniPicker2025`，头文件为 `omnihand/omnipicker_2025.h`。目前没有 Python 绑定。

## 连接

| 连接方式 | 工厂方法 | 平台 |
|----------|----------|------|
| ZLG USB CANFD | `createHandByZlgcan(hand_type, hand_device_id, canfd_device_id, canfd_channel_id)` | Windows / Linux |
| HCAN USB CANFD | `createHandByHcan(hand_type, hand_device_id, canfd_device_id, canfd_channel_id)` | Windows / Linux |
| SocketCAN | `createHandSocketCan(hand_type, hand_device_id, "can0")` | Linux |
| USB 直连 | `createHandByUSB()` | Windows / Linux |
| RS485 | `createHandByRs485(hand_type, hand_device_id, serial_port, baudrate)` | Windows / Linux |

CAN 连接需传入夹爪实际节点 ID，默认值为 `OmniPicker2025::kDefaultHandDeviceId`（1）。适配器和通道索引从 0 开始。工厂保留 `hand_type` 参数，示例使用 `HandType::LEFT`。ZLG 和 HCAN 工厂还支持用适配器序列号替代适配器索引。

USB 直连使用 Fibre USB 协议，不传 COM 端口。安装和接线见[快速入门](QUICK_START.md)，Linux CAN 配置见 [SocketCAN 配置](SOCKETCAN_SETUP.md)。

## C++ 示例

各连接方式共用 [demo_omnipicker_2025.cc](../../../cpp/demo/omnipicker_2025/demo_omnipicker_2025.cc)，构建目标为 `demo_omnipicker_2025`：

```shell
./demo_omnipicker_2025 -d usb
./demo_omnipicker_2025 -d usb --serial USB_SERIAL
./demo_omnipicker_2025 -d hcan -i 0 -c 0 --device-id 1
./demo_omnipicker_2025 -d zlgcan -i 0 -c 0 --device-id 1
./demo_omnipicker_2025 -d socketcan --can-if can0 --device-id 1
./demo_omnipicker_2025 -d rs485 -p COM3 --device-id 1
./demo_omnipicker_2025 -d rs485 -p COM3 --device-id 1 --position 128
```

Windows 可执行文件后缀为 `.exe`，Linux 串口可使用 `/dev/ttyUSB0`；SocketCAN 仅适用于 Linux。`--help` 显示完整参数。示例仅演示常规控制与信息读取，不包含固件更新、标定、标零、写 SN 或出厂参数写入。

USB 使用 `--serial` 选择设备，不接受 COM 端口。USB 通过预设手势发送半开目标（底层为 MIT 控制）；CAN 发送单节点半开目标并打印应答；RS485 不传 `--position` 时只读取信息，传入 0–255 时控制开合并打印应答。CAN／RS485 应答中的位置在指令到达时采样，不代表已到达目标；USB 不提供实测位置反馈。

编译方式参见 [CMakeLists.txt](../../../cpp/demo/omnipicker_2025/CMakeLists.txt)。

## RS485 控制与反馈

RS485 使用固件的自定义串口协议，默认 115200、8N1，不使用 Modbus。设备需启用 `rs485_mode`、关闭 `modbus_mode`；波特率必须与固件配置一致。每个 SDK 实例独占一个串口，多设备请使用不同端口。

硬件前置条件：按[官网用户手册第 5.2 节](https://cn.agilink-ai.com/OmniPicker/20.html)，带拨码开关的 V1.2 控制板默认使用 CAN/CAN-FD。切换 RS485 时应先断电，将 1、2 拨到 ON，3–6 全部拨到 OFF，再上电。拨码选择电气通道，SDK 无法代替硬件切换。

固件参数通过 USB 配置，修改后需失能、保存配置并重启。3.3.5 及之后的固件还需确认 `modbus_mode=False`、`uart_stopbits=0`，使协议和停止位匹配本接口；更早版本的串口使用固定 115200、8N1。

```cpp
auto picker = OmniPicker2025::createHandByRs485(
    HandType::LEFT, 1, "COM3", 115200);  // Linux: "/dev/ttyUSB0"
if (!picker) {
  return 1;
}
const auto feedback = picker->SendFrameSyncByRs485({128, 1, 1, 1, 1});
if (feedback) {
  std::cout << ToString(*feedback) << '\n';
}
```

创建时会查询指定节点的信息，串口打开失败或节点未响应时返回空指针。节点 ID 范围为 1–127，传 0 使用默认 ID 1，不进行广播发现。RS485 地址沿用固件的 `can_node_id`；通过 USB 的 `SetCanNodeId()` 修改时会自动保存配置，重新上电生效。

`SendFrameSyncByRs485()` 复用下文的 `Op1CanfdCtrlFrame` 和 `Op1CanfdStateFrame`，超时或通信失败返回 `std::nullopt`。默认接收超时为 100 ms，可用 `SetFrameRecvTimeout()` 调整；接口同步等待，请在应用工作线程中调用。`ShowDeviceInfo()` 返回设备类型、节点 ID 和固件版本，其他字段保持默认值。

官网 RS485 下行表格的速度、力矩列顺序与固件不一致；SDK 按固件及 CAN 负载定义使用“位置、速度、力矩、加速度、减速度”的顺序。

发送运动命令前请确保固件已完成归零。未归零时固件会返回设备信息，串口协议没有类型字段，无法可靠地将其与状态反馈区分。

## CANFD 控制与反馈

下面以最低速度和力度档位发送打开命令，再检查返回状态。实际使用时可在接口返回的范围内调整速度和力度。

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

`Init()` 检查适配器初始化状态，不代表夹爪已经响应。`SendFrameSyncByCanfd()` 返回 `std::optional<Op1CanfdStateFrame>`；超时、通信失败或回包长度不合法时返回 `std::nullopt`。

### 命令字段

| `Op1CanfdCtrlFrame` 字段 | 范围 | 含义 |
|------------------------|------|------|
| `pos_cmd` | 0–255 | 0 为完全闭合，255 为完全打开 |
| `vel_cmd` | 1–255 | 速度档位 |
| `force_cmd` | 1–255 | 夹持力度档位，范围对应 `GetCanfdFrameRange().tor` |
| `acc_cmd` | 1–255 | 加速度档位 |
| `dec_cmd` | 1–255 | 减速度档位 |

以上均为协议编码值，不是毫米、弧度或物理力单位。`GetCanfdFrameRange()` 提供各字段范围；速度、力度、加速度和减速度的默认档位均为 255。加速度和减速度不能为 0。

### 反馈字段

`Op1CanfdStateFrame` 包含 `fault_code`、`state`、`pos`、`vel`、`force`。位置、速度和力度均为原始反馈字节。`ToString(*feedback)` 可将整帧状态格式化为可读文本。

| `Op1State` | 含义 |
|------------|------|
| `ARRIVERED` | 已到达目标位置 |
| `MOVING` | 正在向目标运动 |
| `BLOCKED` | 接触物体或受到阻挡 |
| `FALLED` | 运动失败 |

以上枚举名沿用 SDK 中的实际拼写。收到回包不代表运动完成，需检查 `state` 是否为 `Op1State::ARRIVERED`。运动过程中可重复发送相同目标命令，获取更新的反馈。

`Op1FaultCode::NO_FAULT` 表示无故障。其他故障码为 `OVER_TEMPTURE`、`OVER_SPEED`、`INIT_ERROR`、`OVER_LIMIT`；可用 `ToString(static_cast<Op1FaultCode>(feedback->fault_code))` 获取文字说明。

## 预设开合位置

CAN、RS485 和 USB 均支持带类型的 `OmniPicker2025Gesture` 接口：

| 手势枚举 | 目标开合比例 |
|----------|--------------|
| `OMNIPICKER_2025_GESTURE_ZERO` | 0.0，完全闭合 |
| `OMNIPICKER_2025_GESTURE_HALF_OPEN` | 0.5，半开 |
| `OMNIPICKER_2025_GESTURE_FULL_OPEN` | 1.0，完全打开 |

```cpp
const auto gesture = OmniPicker2025Gesture::OMNIPICKER_2025_GESTURE_FULL_OPEN;
const float target_ratio = picker->GetHandGesture(gesture);
const float result = picker->SetHandGesture(gesture);
```

`GetHandGesture()` 只返回预设目标比例，不查询设备。CAN 和 RS485 的 `SetHandGesture()` 返回反馈开合比例，失败返回 -1；USB 返回请求比例，不是实测反馈。返回值均不能用于判断已经到位。

CAN 预设手势使用 255 档的速度、力度、加速度和减速度。需要自行控制这些参数时，使用 `SendFrameSyncByCanfd()`。

## USB 直连

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

`SendFrameSyncByCanfd()` 和 `GetCanfdFrameRange()` 仅供 CAN 使用，在 USB 对象上调用会抛出 `std::logic_error`。当前 SDK 销毁 USB 对象时会重启设备，使用期间应保持对象存活。示例的程序返回值不表示打开动作已经完成。

`ShowDeviceInfo()` 读取设备信息：CAN 仅填充设备类型和固件版本；USB 提供更多设备信息。CAN 未查询的字段保留默认值。
