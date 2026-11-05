#ifndef AGILINK_OMNIHAND_CLIENT_WEBSOCKET_CLIENT_H
#define AGILINK_OMNIHAND_CLIENT_WEBSOCKET_CLIENT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "omnihand_client/client_types.h"

namespace agilink {
namespace omnihand {
namespace client {

// Synchronous WebSocket client for commands and stream subscriptions exposed
// by OmniHandServer. The connection is opened lazily by the first operation.
// Network, framing, and JSON implementation details stay behind Pimpl.
class OmniHandWebSocketClient {
 public:
  OmniHandWebSocketClient();
  OmniHandWebSocketClient(std::string host, std::uint16_t port = 8000,
                          int timeout_ms = 10000);
  explicit OmniHandWebSocketClient(OmniHandClientOptions options);
  ~OmniHandWebSocketClient();

  OmniHandWebSocketClient(const OmniHandWebSocketClient&) = delete;
  OmniHandWebSocketClient& operator=(const OmniHandWebSocketClient&) = delete;
  OmniHandWebSocketClient(OmniHandWebSocketClient&&) noexcept;
  OmniHandWebSocketClient& operator=(OmniHandWebSocketClient&&) noexcept;

  void Connect();
  void Close() noexcept;
  bool IsConnected() const noexcept;

  // Sends a protocol request. fields must be an object; type and request_id
  // are assigned by the client. Server-side failures throw
  // OmniHandClientError with status_code() == 0 and the complete WebSocket
  // result message in response().
  Value Request(const std::string& type,
                Value fields = Value::EmptyObject());

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

  // Returns the next {type:"event", ...} message. A negative timeout uses
  // OmniHandClientOptions::timeout_ms.
  Value NextEvent(int timeout_ms = -1);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace client
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_CLIENT_WEBSOCKET_CLIENT_H
