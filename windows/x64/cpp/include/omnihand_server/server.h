#ifndef AGILINK_OMNIHAND_SERVER_SERVER_H
#define AGILINK_OMNIHAND_SERVER_SERVER_H

#include <cstdint>
#include <memory>
#include <string>

namespace agilink {
namespace omnihand {
namespace server {

// Configuration shared by the standalone executable and embedded users.
// Port 0 asks the operating system to choose an available port.
struct ServerOptions {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8000;
  int publish_interval_ms = 100;
  std::string log_level = "info";
  std::string static_file_root;
};

// Embeddable REST/WebSocket server. Implementation details (Crow, routing,
// JSON codecs and device-session management) are intentionally hidden.
class OmniHandServer {
 public:
  OmniHandServer();
  explicit OmniHandServer(std::uint16_t port);
  explicit OmniHandServer(ServerOptions options);
  ~OmniHandServer();

  OmniHandServer(const OmniHandServer&) = delete;
  OmniHandServer& operator=(const OmniHandServer&) = delete;
  OmniHandServer(OmniHandServer&&) noexcept;
  OmniHandServer& operator=(OmniHandServer&&) noexcept;

  // Starts the server in the background and waits until its listening socket
  // is ready. Throws if startup fails.
  void Start();

  // Blocks until a started server exits. Any asynchronous server failure is
  // rethrown to the caller.
  void Wait();

  // Stops the server and waits for its worker threads to finish. Safe to call
  // when the server is already stopped.
  void Stop();

  bool IsRunning() const noexcept;
  std::uint16_t Port() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_SERVER_SERVER_H
