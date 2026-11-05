# OmniHand WebSocket API

## Overview

The OmniHand WebSocket server implements [RFC 6455](https://datatracker.ietf.org/doc/html/rfc6455)
over TCP. All messages are UTF-8 JSON. Clients send **masked** frames (required by RFC 6455 for
client→server). The server sends unmasked frames.

Every request includes a `request_id` field. The server echoes it back in the response so the
client can match responses to requests.

**Default port:** `8000` (shared with REST)

---

## Server

REST and WebSocket are hosted by the same native `OmniHandServer` instance and
share one session store.

```bash
omnihand_server --host 0.0.0.0 --port 8000
# Equivalent Python entry point:
python -m omnihand_server --host 0.0.0.0 --port 8000
```

**WebSocket endpoint:** `ws://host:8000/ws`

See [OmniHand Server](API_SERVER.md) for the C++ and Python embedding APIs and
the black-box test design.

---

## C++ Client — `OmniHandWebSocketClient`

### Header

```cpp
#include "omnihand_client/websocket_client.h"

using agilink::omnihand::client::OmniHandWebSocketClient;
```

### Options

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

### Example

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

The public API uses the standard-library-only `Value` type. Server-side errors
throw `OmniHandClientError`; for WebSocket errors `status_code()` is `0` and
`response()` contains the complete result envelope.

---

## Python Client

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

Install the optional transport dependency with `pip install omnihand[client]`.

---

## Message Reference

### Request envelope

```json
{
  "type": "<message type>",
  "request_id": 42,
  ...
}
```

`request_id` may be any JSON value (int, string). The server echoes it back unchanged.

### Response envelope

```json
{
  "type": "result",
  "request_id": 42,
  "ok": true,
  "result": { ... }
}
```

On error:
```json
{
  "type": "result",
  "request_id": 42,
  "ok": false,
  "error": "Human-readable error message"
}
```

### Message types

| `type` | Direction | Description |
|--------|-----------|-------------|
| `list` | C→S | List all registered hand sessions |
| `create` | C→S | Create a new hand session |
| `remove` | C→S | Remove a hand session |
| `describe` | C→S | Get hand details |
| `methods` | C→S | List callable methods for a hand or product type |
| `call` | C→S | Invoke a method on a hand |
| `subscribe` | C→S | Subscribe to a data stream |
| `unsubscribe` | C→S | Unsubscribe from a stream |
| (push) | S→C | Subscription data push (no `request_id`) |

### `create` request

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

Response `result`:
```json
{ "hand_id": 1, "product_type": "omnihand_2025" }
```

If a session with the same connection parameters already exists, the server returns the existing
session instead of creating a new one, with an additional `"already_exists": true` field.

For `rs485` or `usb` connections (`omnihand_2025` only), use `uart_port` in `conn_config`:
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

### `call` request

```json
{
  "type": "call",
  "request_id": 5,
  "hand_id": 1,
  "method": "get_all_joint_positions",
  "params": {}
}
```

### Available methods

Use `{"type": "methods", "hand_id": <id>}` to query the full list for a connected hand, or
`{"type": "methods", "product_type": "<key>"}` to query statically without creating a session.

| Category | Method | Params | Condition |
|----------|--------|--------|-----------|
| info | `get_vendor_info` | — | always |
| info | `get_device_info` | — | always |
| info | `get_joint_names` | — | always |
| info | `get_sensor_order` | — | supports tactile |
| position | `get_all_joint_positions` | — | always |
| position | `get_joint_position` | `joint_motor_index` | not UMI |
| position | `set_joint_position` | `joint_motor_index`, `posi` | not UMI |
| position | `set_all_joint_positions` | `vec_posi` | not UMI |
| position | `get_all_active_joint_angles` | — | not UMI / H3L |
| position | `set_all_active_joint_angles` | `angles` | not UMI / H3L |
| velocity | `get_all_joint_velocities` | — | not UMI |
| velocity | `get_joint_velocity` | `joint_motor_index` | not UMI |
| velocity | `set_joint_velocity` | `joint_motor_index`, `velo` | not UMI |
| velocity | `set_all_joint_velocities` | `vec_velo` | not UMI |
| control_mode | `get_control_mode` | `joint_motor_index` | O12 / H3UM |
| control_mode | `set_control_mode` | `joint_motor_index`, `mode` | O12 / H3UM |
| control_mode | `get_all_control_modes` | — | O12 / H3UM |
| control_mode | `set_all_control_modes` | `ctrl_modes` | O12 / H3UM |
| mix_ctrl | `mix_control_by_pt` | `positions`, `torques` | O10 / O12 / H3L |
| mix_ctrl | `mix_control_by_pvt` | `positions`, `velocities`, `torques` | O10 / O12 / H3L |
| gesture | `set_hand_gesture` | `gesture` | always |
| gesture | `get_hand_gesture` | `gesture` | always |
| current_threshold | `get_current_threshold` | `joint_motor_index` | not UMI |
| current_threshold | `set_current_threshold` | `joint_motor_index`, `current_threshold` | not UMI |
| current_threshold | `get_all_current_thresholds` | — | not UMI |
| current_threshold | `set_all_current_thresholds` | `current_thresholds` | not UMI |
| temperature | `get_temperature_report` | `joint_motor_index` | not UMI |
| temperature | `get_all_temperature_reports` | — | not UMI |
| current | `get_current_report` | `joint_motor_index` | not UMI |
| current | `get_all_current_reports` | — | not UMI |
| error | `get_error_report` | `joint_motor_index` | not UMI |
| error | `get_all_error_reports` | — | not UMI |
| tactile | `get_tactile_sensor_data` | `finger_index` | O10 (1D) |
| tactile | `get_tactile_sensor_data_raw` | `finger_index` | O10 / H3L (1D) |
| tactile | `get_all_tactile_sensor_data_raw` | — | O10 / H3L (1D) |
| tactile | `get_tactile_sensor_3d_data` | `finger_index` | O12 (3D) |
| communication | `set_request_interval` | `milliseconds` | always |
| communication | `get_request_interval` | — | always |
| communication | `set_frame_recv_timeout` | `milliseconds` | always |
| communication | `get_frame_recv_timeout` | — | always |
| communication | `show_data_details` | `show` | always |
| communication | `set_device_id` | `device_id` | always |

`params` may be omitted or set to `{}` for methods that take no arguments.

### `methods` request — by hand session

```json
{ "type": "methods", "request_id": 3, "hand_id": 1 }
```

Response `result` includes `hand_id`, `product_type`, `methods` array, and `streams` array.

### `methods` request — by product type (no session required)

Query available methods for any product type without connecting a physical device:

```json
{ "type": "methods", "request_id": 3, "product_type": "omnihand_2025" }
```

Valid `product_type` values: `omnihand_2025`, `omnihand_pro_2025`, `omnihand_dex_umi`,
`omnihand_3_lite`, `omnihand_3_ultra_m`.

Response `result`:
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

### `subscribe` / `unsubscribe`

```json
{
  "type": "subscribe",
  "request_id": 10,
  "hand_id": 1,
  "streams": ["state"]
}
```

Available streams: `state` (joint positions/velocities), `diagnostics` (temperature/current/errors),
`tactile` (tactile sensor data, if supported by the product).

Push messages (no `request_id`, arrive at `publish_interval_ms` rate):
```json
{
  "hand_id": 1,
  "stream": "state",
  "data": { "joint_positions": [0.1, 0.2, ...] }
}
```

---

## Demo

### C++ client

Link `omnihand_client` and use the `OmniHandWebSocketClient` example above.
No Crow or WebSocket implementation header is required by the application.

### Python client

```bash
pip install "omnihand[client]"
python python/demo/server/demo_ws_client.py --host 127.0.0.1 --port 8000 -d zlgcan
# rs485 / usb:
python python/demo/server/demo_ws_client.py --host 127.0.0.1 --port 8000 -d rs485 --uart-port /dev/ttyUSB0
```

Source: `python/demo/server/demo_ws_client.py`

## Test

### C++ tests (no device required)

```bash
# WebSocket public contract only:
./test_omnihand_server --gtest_filter=*WebSocketClientBlackBoxTest*

# All WebSocket and REST tests:
./test_omnihand_server
```

Source: `cpp/test/test_omnihand_server.cc`

### Python tests (no device required)

```bash
python -m pytest python/test/test_omnihand_server.py -v
```

Source: `python/test/test_omnihand_server.py`

---

## Related Documentation

- [Server Interface Overview](API_SERVER.md)
- [REST API](API_SERVER_REST.md)
- [C++ API Reference](API_CPP.md)
