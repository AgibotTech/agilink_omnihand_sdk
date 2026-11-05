# OmniHand WebSocket API

## 概述

OmniHand WebSocket 服务器基于 TCP 实现了 [RFC 6455](https://datatracker.ietf.org/doc/html/rfc6455) 协议。
所有消息均为 UTF-8 JSON 格式。客户端发送**带掩码**的帧（RFC 6455 对客户端→服务器方向的强制要求），
服务器发送**不带掩码**的帧。

每个请求包含 `request_id` 字段，服务器在响应中原样返回，客户端用于匹配请求与响应。

**默认端口：** `8000`（与 REST 共用）

---

## Server

REST 和 WebSocket 由同一个原生 `OmniHandServer` 实例提供，并共享设备会话：

```bash
omnihand_server --host 0.0.0.0 --port 8000
# 等价的 Python 原生入口：
python -m omnihand_server --host 0.0.0.0 --port 8000
```

**WebSocket 端点：** `ws://host:8000/ws`

C++ 与 Python 的嵌入方式见 [OmniHand Server](API_SERVER.md)。

---

## C++ 客户端 — `OmniHandWebSocketClient`

### 头文件

```cpp
#include "omnihand_client/websocket_client.h"

using agilink::omnihand::client::OmniHandWebSocketClient;
```

### 配置项

```cpp
struct OmniHandClientOptions {
  std::string host = "127.0.0.1";
  std::uint16_t port = 8000;
  int timeout_ms = 10000;
};
```

### API

```cpp
OmniHandWebSocketClient();
OmniHandWebSocketClient(std::string host, std::uint16_t port = 8000,
                        int timeout_ms = 10000);
explicit OmniHandWebSocketClient(OmniHandClientOptions options);

void Connect();
void Close();
bool IsConnected() const;

Value Request(const std::string& type, Value fields = Value::EmptyObject());
Value ListHands();
Value CreateHand(const std::string& product_type,
                 const std::string& connection_type,
                 Value connection_config = Value::EmptyObject(),
                 const std::string& hand_side = "left");
Value DescribeHand(int hand_id);
Value RemoveHand(int hand_id);
Value Methods(int hand_id);
Value MethodsForProduct(const std::string& product_type);
Value Call(int hand_id, const std::string& method,
           Value params = Value::EmptyObject());
Value Subscribe(int hand_id, const std::vector<std::string>& streams);
Value Unsubscribe(int hand_id, const std::vector<std::string>& streams);
Value NextEvent(int timeout_ms = -1);
```

### 示例

```cpp
OmniHandWebSocketClient client("127.0.0.1", 8000);
client.Connect();

auto hands = client.ListHands();
auto methods = client.MethodsForProduct("omnihand_2025");
if (!hands.Empty()) {
  const int hand_id = static_cast<int>(
      hands.AsArray().front().At("hand_id").AsInt());
  client.Subscribe(hand_id, {"state", "tactile"});
  auto event = client.NextEvent();
  client.Unsubscribe(hand_id, {"state", "tactile"});
}
client.Close();
```

公共 API 使用仅依赖 STL 的 `Value`。服务端错误会抛出
`OmniHandClientError`；WebSocket 错误的 `status_code()` 为 `0`，
`response()` 保留完整的结果封包。

---

## Python 客户端

```python
import asyncio
from omnihand_client import OmniHandWebSocketClient

async def demo():
    async with OmniHandWebSocketClient("127.0.0.1", 8000) as client:
        hands = await client.list_hands()
        print(hands)
        if hands:
            hand_id = hands[0]["hand_id"]
            await client.subscribe(hand_id, ["state", "tactile"])
            print(await client.next_event())

asyncio.run(demo())
```

使用 `pip install omnihand[client]` 安装可选的传输依赖。

---

## 消息协议参考

### 请求封包格式

```json
{
  "type": "<消息类型>",
  "request_id": 42,
  ...其他字段...
}
```

`request_id` 可以是任意 JSON 值（整数或字符串），服务器原样返回。

### 响应封包格式

```json
{
  "type": "result",
  "request_id": 42,
  "ok": true,
  "result": { ... }
}
```

出错时：
```json
{
  "type": "result",
  "request_id": 42,
  "ok": false,
  "error": "可读的错误信息"
}
```

### 消息类型

| `type` | 方向 | 说明 |
|--------|------|------|
| `list` | 客户端→服务器 | 列出所有已注册的手部会话 |
| `create` | 客户端→服务器 | 创建新的手部会话 |
| `remove` | 客户端→服务器 | 移除手部会话 |
| `describe` | 客户端→服务器 | 获取手部详情 |
| `methods` | 客户端→服务器 | 列出指定手部或产品类型的可调用方法 |
| `call` | 客户端→服务器 | 调用手部的某个方法 |
| `subscribe` | 客户端→服务器 | 订阅数据流 |
| `unsubscribe` | 客户端→服务器 | 取消订阅数据流 |
| （推送帧） | 服务器→客户端 | 订阅数据推送（无 `request_id`） |

### `create` 请求

```json
{
  "type": "create",
  "request_id": 1,
  "product_type": "omnihand_2025",
  "conn_method": "zlgcan",
  "hand_side": "left",
  "conn_config": {
    "canfd_device_id": 0,
    "canfd_channel_id": 0
  }
}
```

响应中的 `result` 字段：
```json
{ "hand_id": 1, "product_type": "omnihand_2025" }
```

若相同连接参数的会话已存在，服务器直接返回已有会话，并附加 `"already_exists": true` 字段，不会重复创建。

使用 `rs485` 或 `usb` 连接时（仅 `omnihand_2025` 支持），`conn_config` 使用 `uart_port`：
```json
{
  "type": "create",
  "request_id": 2,
  "product_type": "omnihand_2025",
  "conn_method": "rs485",
  "hand_side": "left",
  "conn_config": { "uart_port": "/dev/ttyUSB0" }
}
```

### `call` 请求

```json
{
  "type": "call",
  "request_id": 5,
  "hand_id": 1,
  "method": "get_all_joint_positions",
  "params": {}
}
```

### 可调用方法

运行时可通过 `{"type": "methods", "hand_id": <id>}` 查询已连接手部的完整方法列表，
或通过 `{"type": "methods", "product_type": "<key>"}` 在无需创建会话的情况下静态查询。

### `methods` 请求 — 按手部会话

```json
{ "type": "methods", "request_id": 3, "hand_id": 1 }
```

响应 `result` 包含 `hand_id`、`product_type`、`methods` 数组和 `streams` 数组。

### `methods` 请求 — 按产品类型（无需会话）

无需连接物理设备，即可查询指定产品类型的可用方法：

```json
{ "type": "methods", "request_id": 3, "product_type": "omnihand_2025" }
```

有效的 `product_type` 值：`omnihand_2025`、`omnihand_pro_2025`、`omnihand_dex_umi`、
`omnihand_3_lite`、`omnihand_3_ultra_m`。

响应 `result`：
```json
{
  "hand_id": null,
  "product_type": "omnihand_2025",
  "methods": [
    { "name": "get_vendor_info", "category": "info" },
    { "name": "set_all_joint_positions", "category": "position", "params": ["vec_posi"] }
  ],
  "streams": ["state", "diagnostics", "tactile"]
}
```

| 类别 | 方法名 | 参数 | 适用条件 |
|------|--------|------|----------|
| info | `get_vendor_info` | — | 全产品 |
| info | `get_device_info` | — | 全产品 |
| info | `get_joint_names` | — | 全产品 |
| info | `get_sensor_order` | — | 支持触觉传感器 |
| position | `get_all_joint_positions` | — | 全产品 |
| position | `get_joint_position` | `joint_motor_index` | 非 UMI |
| position | `set_joint_position` | `joint_motor_index`, `posi` | 非 UMI |
| position | `set_all_joint_positions` | `vec_posi` | 非 UMI |
| position | `get_all_active_joint_angles` | — | 非 UMI / H3L |
| position | `set_all_active_joint_angles` | `angles` | 非 UMI / H3L |
| velocity | `get_all_joint_velocities` | — | 非 UMI |
| velocity | `get_joint_velocity` | `joint_motor_index` | 非 UMI |
| velocity | `set_joint_velocity` | `joint_motor_index`, `velo` | 非 UMI |
| velocity | `set_all_joint_velocities` | `vec_velo` | 非 UMI |
| control_mode | `get_control_mode` | `joint_motor_index` | O12 / H3UM |
| control_mode | `set_control_mode` | `joint_motor_index`, `mode` | O12 / H3UM |
| control_mode | `get_all_control_modes` | — | O12 / H3UM |
| control_mode | `set_all_control_modes` | `ctrl_modes` | O12 / H3UM |
| mix_ctrl | `mix_control_by_pt` | `positions`, `torques` | O10 / O12 / H3L |
| mix_ctrl | `mix_control_by_pvt` | `positions`, `velocities`, `torques` | O10 / O12 / H3L |
| gesture | `set_hand_gesture` | `gesture` | 全产品 |
| gesture | `get_hand_gesture` | `gesture` | 全产品 |
| current_threshold | `get_current_threshold` | `joint_motor_index` | 非 UMI |
| current_threshold | `set_current_threshold` | `joint_motor_index`, `current_threshold` | 非 UMI |
| current_threshold | `get_all_current_thresholds` | — | 非 UMI |
| current_threshold | `set_all_current_thresholds` | `current_thresholds` | 非 UMI |
| temperature | `get_temperature_report` | `joint_motor_index` | 非 UMI |
| temperature | `get_all_temperature_reports` | — | 非 UMI |
| current | `get_current_report` | `joint_motor_index` | 非 UMI |
| current | `get_all_current_reports` | — | 非 UMI |
| error | `get_error_report` | `joint_motor_index` | 非 UMI |
| error | `get_all_error_reports` | — | 非 UMI |
| tactile | `get_tactile_sensor_data` | `finger_index` | O10（1D） |
| tactile | `get_tactile_sensor_data_raw` | `finger_index` | O10 / H3L（1D） |
| tactile | `get_all_tactile_sensor_data_raw` | — | O10 / H3L（1D） |
| tactile | `get_tactile_sensor_3d_data` | `finger_index` | O12（3D） |
| communication | `set_request_interval` | `milliseconds` | 全产品 |
| communication | `get_request_interval` | — | 全产品 |
| communication | `set_frame_recv_timeout` | `milliseconds` | 全产品 |
| communication | `get_frame_recv_timeout` | — | 全产品 |
| communication | `show_data_details` | `show` | 全产品 |
| communication | `set_device_id` | `device_id` | 全产品 |

无参数的方法可省略 `params` 字段，或传入 `{}`。

### `subscribe` / `unsubscribe`

```json
{
  "type": "subscribe",
  "request_id": 10,
  "hand_id": 1,
  "streams": ["state"]
}
```

可用数据流：`state`（关节位置/速度）、`diagnostics`（温度/电流/错误）、
`tactile`（触觉传感器数据，仅部分产品支持）。

推送消息（无 `request_id`，以 `publish_interval_ms` 为间隔推送）：
```json
{
  "hand_id": 1,
  "stream": "state",
  "data": { "joint_positions": [0.1, 0.2, ...] }
}
```

---

## Demo 示例

### C++ 客户端

链接 `omnihand_client`，使用上面的 `OmniHandWebSocketClient` 示例即可。
应用无需包含 Crow 或 WebSocket 实现层头文件。

### Python 客户端

```bash
pip install "omnihand[client]"
python python/demo/server/demo_ws_client.py --host 127.0.0.1 --port 8000 -d zlgcan
# rs485 / usb：
python python/demo/server/demo_ws_client.py --host 127.0.0.1 --port 8000 -d rs485 --uart-port /dev/ttyUSB0
```

源码：`python/demo/server/demo_ws_client.py`

## 测试

### C++ 测试（无需实际设备）

```bash
# 只运行 WebSocket 公共契约测试：
./test_omnihand_server --gtest_filter=*WebSocketClientBlackBoxTest*

# 运行所有 WebSocket 和 REST 测试：
./test_omnihand_server
```

源码：`cpp/test/test_omnihand_server.cc`

### Python 测试（无需实际设备）

```bash
python -m pytest python/test/test_omnihand_server.py -v
```

源码：`python/test/test_omnihand_server.py`

---

## 相关文档

- [服务器接口总览](API_SERVER.md)
- [REST API](API_SERVER_REST.md)
- [C++ API 参考](API_CPP.md)
