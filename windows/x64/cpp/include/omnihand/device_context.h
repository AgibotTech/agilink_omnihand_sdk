// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#ifndef AGILINK_OMNIHAND_DEVICE_CONTEXT_H_
#define AGILINK_OMNIHAND_DEVICE_CONTEXT_H_

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "omnihand/export_symbols.h"
#include "omnihand/utils.h"

namespace agilink {
namespace omnihand {

// SDK-defined category; severity is specified independently on each record.
inline constexpr uint16_t kCommunicationLogCategory = 1;

struct CanConnectionInfo {
  enum class UsbCanAdapter {
    ZLG,
    HCAN,
  };
  UsbCanAdapter adapter = UsbCanAdapter::ZLG;
  uint8_t adapter_index = 0;
  uint8_t channel = 0;
  std::string serial_number;
};

struct SerialConnectionInfo {
  enum class Mode {
    USB,
    RS485,
  };
  Mode mode = Mode::USB;
  std::string port;
  uint32_t baud_rate = 0;
};

struct TcpCanConnectionInfo {
  std::string host;
  uint16_t port = 0;
  uint8_t channel = 0;
};

// Per-hand runtime state; connection descriptions are immutable after creation.
struct AGIBOT_EXPORT DeviceContext {
  DeviceContext();
  ~DeviceContext();
  DeviceContext(const DeviceContext&) = delete;
  DeviceContext& operator=(const DeviceContext&) = delete;

  // Replaces the asynchronous device log callback; empty restores default output.
  void SetLogCallback(DataDetailsLogCallback callback) const;

  // Unique session source, independent of protocol device IDs.
  const uint64_t source_id;

  // Effective SDK addressing IDs; zero private ID means unavailable/unknown.
  std::atomic<uint16_t> device_id{0};
  std::atomic<uint16_t> private_device_id{0};
  // Connection descriptions are initialized before publishing the hand.
  std::optional<CanConnectionInfo> can;
  std::optional<SerialConnectionInfo> serial;
  std::optional<TcpCanConnectionInfo> tcp_can;
  std::optional<std::string> socket_can;
  bool native_usb = false;
};

}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_DEVICE_CONTEXT_H_
