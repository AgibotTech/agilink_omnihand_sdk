# OmniHand Server

The SDK provides one native server implementation that exposes REST and
WebSocket on the same port. The standalone executable, embedded C++ API, and
Python binding all use this implementation and therefore share the same routes,
session store, and protocol behavior.

## Design

The public C++ surface separates hosting from remote access:

- `omnihand_client` — lightweight remote client without hardware SDK dependencies;
- `omnihand_server` — embeddable static library;
- `omnihand_server/server.h` — `ServerOptions` and `OmniHandServer`;
- `omnihand_server` executable — command-line host built from the same library.

Crow, ASIO, routers, handlers, JSON codecs, and SDK device adapters are private
implementation details. They are neither included by the public header nor
installed as public server headers.

```text
Application / Python binding
            |
   OmniHandServer (Pimpl)
            |
 REST + WebSocket transport
            |
 shared session/device service
            |
        libomnihand
```

Remote applications link `omnihand_client` instead. They do not link
`omnihand`, CAN drivers, Crow, or the Server implementation.

`Start()` waits until the listening socket is ready. `Stop()` is idempotent and
waits for server workers to finish. `Wait()` supports process-style blocking.
A port value of `0` lets the operating system select an available port; retrieve
the selected value through `Port()`/`server.port` after startup.

## Standalone executable

```bash
omnihand_server --port 8000
```

REST is available below `/v1`, WebSocket at `/ws`, health at `/health`, and the
packaged OpenAPI page at `/docs/` when frontend assets can be located.

## Web frontend

After starting the Server, open:

- Demo UI: <http://127.0.0.1:8000/>
- OpenAPI: <http://127.0.0.1:8000/docs/>

The Demo UI uses the same REST and WebSocket endpoints to create a hand, call
methods, and view subscribed data. Frontend assets are included by the SDK
installer and Python wheel.

## Embedded C++

```cmake
if(WIN32)
  list(APPEND CMAKE_MODULE_PATH "C:/omnihand/share/cmake/omnihand")
else()
  list(APPEND CMAKE_MODULE_PATH "/usr/local/share/cmake/omnihand")
endif()
find_package(omnihand REQUIRED)

add_executable(my_server main.cpp)
target_link_libraries(my_server PRIVATE omnihand_server)
```

```cpp
#include "omnihand_server/server.h"

int main() {
  agilink::omnihand::server::OmniHandServer server(8000);
  server.Start();
  server.Wait();
}
```

## C++ client

```cmake
find_package(omnihand REQUIRED)
add_executable(my_client main.cpp)
target_link_libraries(my_client PRIVATE omnihand_client)
```

```cpp
#include "omnihand_client/rest_client.h"

using agilink::omnihand::client::OmniHandRestClient;
OmniHandRestClient client("127.0.0.1", 8000);

auto products = client.ListProductTypes();
auto methods = client.MethodsForProduct("omnihand_2025");
```

`CreateHand()`, `DescribeHand()`, `Methods()`, `Call()`, and `RemoveHand()`
cover remote device sessions and generic SDK calls. Parameters and results use
the standard-library-only `Value` type. `OmniHandClientError` preserves the
HTTP status and structured error response for assertions in gtest.

The same library also provides a persistent synchronous WebSocket client for
commands and stream subscriptions:

```cpp
#include "omnihand_client/websocket_client.h"

using agilink::omnihand::client::OmniHandWebSocketClient;
OmniHandWebSocketClient websocket("127.0.0.1", 8000);

auto hands = websocket.ListHands();
if (!hands.Empty()) {
  const int hand_id = static_cast<int>(
      hands.AsArray().front().At("hand_id").AsInt());
  websocket.Subscribe(hand_id, {"state", "tactile"});
  auto event = websocket.NextEvent();
  websocket.Unsubscribe(hand_id, {"state", "tactile"});
}
```

`OmniHandWebSocketClient` performs the RFC 6455 handshake and framing, matches
responses by request ID, handles Ping/Pong, and queues events that arrive while
a command is waiting for its response. Its transport dependencies remain
private behind Pimpl.

REST and WebSocket clients connected to the same Server share one hand session
and one underlying SDK object per `hand_id`. Create the hand once, then reuse
the returned `hand_id` from either protocol. SDK calls, including subscription
polls, are serialized per hand. Concurrent create requests for the same
connection are also serialized and return the existing session. Do not run two
Server processes, or a Server and a direct SDK owner, against the same physical
CAN/serial endpoint.

## Embedded Python

Python is intentionally a thin lifecycle binding; it does not reimplement the
REST/WebSocket routes.

```python
from omnihand import OmniHandServer

server = OmniHandServer(8000)
server.start()
try:
    server.wait()
finally:
    server.stop()
```

The same server can be started from the command line:

```bash
python -m omnihand_server --port 8000
```

## Python client

`OmniHandRestClient` is the public synchronous REST client. It exposes server and
session operations while keeping SDK calls generic, so a newly registered
server method does not require another client wrapper.

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

`OmniHandWebSocketClient` provides asynchronous commands and stream
subscriptions. Install its lightweight optional transport dependency with
`pip install omnihand[client]`.

```python
async with client.websocket() as websocket:
    hands = await websocket.list_hands()
    await websocket.subscribe(hand_id, ["state", "tactile"])
    event = await websocket.next_event()
```

## Test strategy

Server compatibility is tested through public behavior rather than private Crow
router types:

1. Start either the C++ executable or the Python native-server entry point.
2. `test_omnihand_server` links only `omnihand_client` and covers both
   `OmniHandRestClient` and `OmniHandWebSocketClient`; the pytest uses only
   `omnihand_client`. Both test the same external Server and print exact startup
   commands when they begin or cannot connect.
3. The Python test does not implement HTTP or WebSocket framing itself.
4. Hardware-dependent hand creation and motion remain integration tests and are
   run separately with a connected device.

```bash
cmake -S . -B build -DBUILD_CPP_TESTS=ON
cmake --build build --target test_omnihand_server --parallel

# Terminal 1: choose one
omnihand_server --port 8000
python -m omnihand_server --port 8000

# Terminal 2
ctest --test-dir build -R OmniHandServerTest --output-on-failure
python -m pytest python/test/test_omnihand_server.py -v
```

See [REST API](API_SERVER_REST.md) and [WebSocket API](API_SERVER_WS.md) for the
wire protocol.
