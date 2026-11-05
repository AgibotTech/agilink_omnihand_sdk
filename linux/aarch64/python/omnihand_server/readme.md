# OmniHand native server Python facade

This package is a thin facade over the C++ `omnihand_server` library. It does
not contain a second FastAPI implementation.

```python
from omnihand_server import OmniHandServer, ServerOptions

# Binds 0.0.0.0:8000 by default; pass a port to override it.
server = OmniHandServer(9000)
```

Use `ServerOptions` when other settings also need to be changed:

```python
from omnihand_server import OmniHandServer, ServerOptions

options = ServerOptions()
options.host = "127.0.0.1"
options.port = 8000

server = OmniHandServer(options)
server.start()
try:
    server.wait()
finally:
    server.stop()
```

Command-line use:

```bash
python -m omnihand_server --host 127.0.0.1 --port 8000
```

The package also provides a public REST client. Method parameters are regular
JSON-compatible Python values:

```python
from omnihand_client import OmniHandRestClient

client = OmniHandRestClient("127.0.0.1", 8000)
hand = client.create_hand(
    "omnihand_2025",
    "hcan",
    {"hand_device_id": 1, "canfd_device_id": 0, "canfd_channel_id": 0},
)
try:
    positions = hand.call("get_all_joint_positions")
    hand.call("set_all_joint_positions", vec_posi=[2048] * 10)
finally:
    hand.close()
```

For WebSocket commands and stream subscriptions, install the optional client
dependency with `pip install omnihand[client]` and use
`OmniHandWebSocketClient` or `client.websocket()`.

Design and black-box testing details are documented in
`release/doc/en/API_SERVER.md` and `release/doc/zh_cn/API_SERVER.md`.
