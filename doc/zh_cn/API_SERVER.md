# OmniHand Server

SDK 只保留一套原生 Server 实现，在同一个端口同时提供 REST 和 WebSocket。
独立可执行程序、C++ 嵌入接口和 Python 绑定都复用这套实现，因此路由、会话和
协议行为不会出现两份实现不一致的问题。

## 设计

C++ 对外将服务宿主和远程访问拆开：

- `omnihand_client`：轻量远程 Client，不依赖硬件 SDK；
- `omnihand_server`：可嵌入静态库；
- `omnihand_server/server.h`：只包含 `ServerOptions` 和 `OmniHandServer`；
- `omnihand_server` 可执行程序：基于同一个静态库构建的命令行宿主。

Crow、ASIO、Router、Handler、JSON codec 和 SDK device adapter 都是内部实现，
不会出现在公共头文件中，也不会作为 Server 公共头文件安装。

```text
应用程序 / Python 绑定
          |
 OmniHandServer（Pimpl）
          |
 REST + WebSocket 传输层
          |
 共享会话与设备服务
          |
      libomnihand
```

远程应用只链接 `omnihand_client`，不需要链接 `omnihand`、CAN 驱动、Crow 或
Server 实现。

`Start()` 会等待监听端口真正就绪；`Stop()` 可重复调用，并等待工作线程退出；
`Wait()` 用于像独立进程一样阻塞等待。端口设为 `0` 时由操作系统选择可用端口，
启动后通过 `Port()` 或 Python 的 `server.port` 获取实际端口。

## 独立运行

```bash
omnihand_server --port 8000
```

REST 位于 `/v1`，WebSocket 位于 `/ws`，健康检查位于 `/health`。能够找到前端
资源时，OpenAPI 页面位于 `/docs/`。

## 网页前端

启动 Server 后，在浏览器打开：

- 示例页面：<http://127.0.0.1:8000/>
- OpenAPI：<http://127.0.0.1:8000/docs/>

示例页面通过同一个 REST 和 WebSocket 服务创建设备、调用方法并查看订阅数据。
SDK 安装包和 Python wheel 均包含前端资源。

## C++ 嵌入

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

## C++ Client

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

`CreateHand()`、`DescribeHand()`、`Methods()`、`Call()` 和 `RemoveHand()`
覆盖远程设备会话及通用 SDK 调用。参数和返回值使用仅依赖 STL 的 `Value`。
`OmniHandClientError` 会保留 HTTP 状态码和结构化错误响应，便于 gtest 断言。

同一个库还提供持久连接的同步 WebSocket Client，用于命令和数据流订阅：

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

`OmniHandWebSocketClient` 负责 RFC 6455 握手和帧处理、按请求 ID 匹配响应、
处理 Ping/Pong，并缓存等待命令响应期间到达的事件。传输依赖仍通过 Pimpl
保留在实现内部。

连接到同一个 Server 的 REST 与 WebSocket Client 共用设备会话；每个
`hand_id` 只对应一个底层 SDK 对象。只需创建一次手，然后两个协议都复用返回的
`hand_id`。同一只手的 SDK 调用（包括订阅轮询）会串行执行；针对同一连接的并发
创建请求也会串行化，后到请求直接返回已有会话。不要让两个 Server 进程，或
Server 与直接使用 SDK 的程序，同时打开同一个 CAN/串口端点。

## Python 薄封装

Python 只封装原生 Server 的生命周期，不再重新实现一套 REST/WebSocket 路由。

```python
from omnihand import OmniHandServer

server = OmniHandServer(8000)
server.start()
try:
    server.wait()
finally:
    server.stop()
```

也可以从命令行启动同一个原生 Server：

```bash
python -m omnihand_server --port 8000
```

## Python Client

`OmniHandRestClient` 是公开的同步 REST Client。它封装服务管理和设备会话操作，
具体 SDK 调用保留通用的 `call()` 形式。因此 Server 注册新方法时，不必再给
Client 重复增加一层同名函数。

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

`OmniHandWebSocketClient` 用于异步命令和数据流订阅。使用前通过
`pip install omnihand[client]` 安装轻量的可选 WebSocket 依赖。

```python
async with client.websocket() as websocket:
    hands = await websocket.list_hands()
    await websocket.subscribe(hand_id, ["state", "tactile"])
    event = await websocket.next_event()
```

## 测试设计

兼容性测试只依赖公共行为，不再 include Crow Router 等内部头文件：

1. 先启动 C++ 可执行程序或 Python 原生 Server 入口中的任意一个。
2. `test_omnihand_server` 只链接 `omnihand_client`，同时覆盖
   `OmniHandRestClient` 和 `OmniHandWebSocketClient`；pytest 也只使用
   `omnihand_client`。两者测试同一个外部 Server，并会在开始测试或连接失败时
   打印准确的启动命令。
3. Python 测试不再自行实现 HTTP 或 WebSocket 帧。
4. 创建设备和运动控制属于硬件集成测试，连接实际设备后单独执行。

```bash
cmake -S . -B build -DBUILD_CPP_TESTS=ON
cmake --build build --target test_omnihand_server --parallel

# 终端1：任选一个
omnihand_server --port 8000
python -m omnihand_server --port 8000

# 终端2
ctest --test-dir build -R OmniHandServerTest --output-on-failure
python -m pytest python/test/test_omnihand_server.py -v
```

报文协议见 [REST API](API_SERVER_REST.md) 和 [WebSocket API](API_SERVER_WS.md)。
