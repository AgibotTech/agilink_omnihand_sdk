# OmniHandPicker 2025 (OP1) C++ API

[English](../en/API_CPP_OP1.md) | [C++ API 索引](API_CPP.md)

OP1 是 1 自由度夹爪，C++ 接口类为 `agilink::omnihand::OmniPicker2025`，头文件为 `omnihand/omnipicker_2025.h`。目前没有 Python 绑定。

## 连接

| 连接方式 | 工厂方法 | 平台 |
|----------|----------|------|
| ZLG USB CANFD | `createHandByZlgcan(hand_type, hand_device_id, canfd_device_id, canfd_channel_id)` | Windows / Linux |
| HCAN USB CANFD | `createHandByHcan(hand_type, hand_device_id, canfd_device_id, canfd_channel_id)` | Windows / Linux |
| SocketCAN | `createHandSocketCan(hand_type, hand_device_id, "can0")` | Linux |
| ZLG CANFD TCP | `createHandByZlgCanTcp(hand_type, hand_device_id, tcp_host, tcp_port, canfd_channel_id)` | Windows / Linux x64 |
| USB 直连 | `createHandByUSB()` | Windows / Linux |

CAN 连接需传入夹爪实际节点 ID，默认值为 `OmniPicker2025::kDefaultHandDeviceId`（1）。适配器和通道索引从 0 开始。工厂保留 `hand_type` 参数，示例使用 `HandType::LEFT`。ZLG 和 HCAN 工厂还支持用适配器序列号替代适配器索引。

USB 直连使用 Fibre USB 协议，不传 COM 端口；不支持 RS485。安装和接线见[快速入门](QUICK_START.md)，Linux CAN 配置见 [SocketCAN 配置](SOCKETCAN_SETUP.md)。

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

CAN 和 USB 均支持带类型的 `OmniPicker2025Gesture` 接口：

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

`GetHandGesture()` 只返回预设目标比例，不查询设备。CAN 的 `SetHandGesture()` 返回反馈开合比例，失败返回 -1；USB 返回请求比例，不是实测反馈。两者返回值均不能用于判断已经到位。

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
