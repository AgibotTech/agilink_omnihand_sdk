// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#ifndef OMNIHAND_DEMO_DEMO_OPTIONS_H_
#define OMNIHAND_DEMO_DEMO_OPTIONS_H_

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "omnihand/device_context.h"
#include "omnihand/omnihand.h"

namespace demo {

struct Options {
  std::string device;
  std::string hand = "left";
  uint8_t device_id = 1;
  uint8_t adapter_index = 0;
  uint8_t channel = 0;
  std::string serial;
  std::string second_serial;
  std::string port;
  std::string second_port;
  std::string can_if = "can0";
  std::string second_can_if = "can1";
  uint32_t baudrate = 0;
  int request_interval = -1;
  int recv_timeout = -1;
  bool verbose = false;
  int position = -1;
  std::string model = "s";
  std::string revision;
  bool xense = false;
};

inline bool Contains(std::initializer_list<std::string_view> values,
                     std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

inline bool HelpRequested(int argc, char** argv) {
  if (argc == 1) {
    return true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--help" ||
        std::string_view(argv[i]) == "-h") {
      return true;
    }
  }
  return false;
}

inline void PrintUsage(const char* program,
                       std::initializer_list<std::string_view> devices,
                       const char* extra_help = "") {
  std::cout << "Usage: " << program << " [options]\n"
            << "  -d, --device NAME  Transport:";
  for (const auto device : devices) {
    std::cout << ' ' << device;
  }
  std::cout
      << " (default: " << *devices.begin() << ")\n"
      << "  --hand SIDE        left | right | both (default: left)\n"
      << "  --device-id ID     Target ID [1,127]; never writes device ID\n"
      << "  -f MS              Request interval after connection\n"
      << "  -t MS              Receive timeout after connection\n"
      << "  -v, --verbose      Print raw communication frames\n";
  if (Contains(devices, "zlgcan") || Contains(devices, "hcan")) {
    std::cout << "  -i INDEX           CAN adapter index (default: 0)\n"
              << "  -c CHANNEL         CAN channel index (default: 0)\n"
              << "  --serial SERIAL    Select CAN adapter by serial, not -i\n"
              << "  --second-serial S  Second adapter for both mode\n"
              << "Both on one CAN adapter uses CHANNEL and CHANNEL+1;\n"
              << "with two adapter serials, each uses CHANNEL.\n";
  }
  if (Contains(devices, "rs485") || Contains(devices, "usb")) {
    std::cout
        << "  -p, --port PORT    Serial port for RS485 or USB serial\n"
        << "  -b, --baudrate B   Serial baud rate (default: SDK default)\n"
        << "  --second-port PORT Second serial port for both mode\n";
  }
  if (Contains(devices, "socketcan")) {
    std::cout << "  --can-if IFACE     SocketCAN interface (default: can0)\n"
              << "  --second-can-if IF Second interface (default: can1)\n";
  }
  std::cout << extra_help << "  -h, --help         Show this help\n";
}

inline int ParseValue(std::string_view text, int minimum, int maximum) {
  int value = 0;
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
      value < minimum || value > maximum) {
    throw std::invalid_argument("Invalid number: " + std::string(text));
  }
  return value;
}

inline Options ParseOptions(int argc, char** argv,
                            std::initializer_list<std::string_view> devices,
                            std::initializer_list<std::string_view> extras = {},
                            uint8_t default_id = 1) {
  Options options;
  options.device = *devices.begin();
  options.device_id = default_id;
  // Resolve the transport first so option order does not change port handling.
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-d" || arg == "--device") {
      if (++i == argc) {
        throw std::invalid_argument("Missing value for -d");
      }
      options.device = argv[i];
    }
  }
  if (!Contains(devices, options.device)) {
    throw std::invalid_argument("Unsupported transport: " + options.device);
  }
#ifndef __linux__
  if (options.device == "socketcan") {
    throw std::invalid_argument("SocketCAN is only supported on Linux");
  }
#endif
  const bool can = options.device == "zlgcan" || options.device == "hcan";
  const bool serial = options.device == "rs485" || options.device == "usb";
  const bool socketcan = options.device == "socketcan";
  bool index_set = false;
  bool hand_set = false;
  bool second_if_set = false;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      options.verbose = true;
      continue;
    }
    if (arg == "--xense" && Contains(extras, arg)) {
      options.xense = true;
      continue;
    }
    if (arg == "left" || arg == "right" || arg == "both") {
      if (hand_set) {
        throw std::invalid_argument("Select one hand mode");
      }
      options.hand = arg;
      hand_set = true;
      continue;
    }
    if (i + 1 == argc) {
      throw std::invalid_argument("Missing value for " + std::string(arg));
    }
    const std::string_view value = argv[++i];
    if (value.empty()) {
      throw std::invalid_argument("Empty value for " + std::string(arg));
    }
    if (arg == "-d" || arg == "--device") {
      continue;
    }
    if (arg == "--hand" && !hand_set) {
      options.hand = value;
      hand_set = true;
    } else if (arg == "--id" || arg == "--device-id") {
      options.device_id = static_cast<uint8_t>(ParseValue(value, 1, 127));
    } else if (can && arg == "-i") {
      options.adapter_index = static_cast<uint8_t>(ParseValue(value, 0, 255));
      index_set = true;
    } else if (can && arg == "-c") {
      options.channel = static_cast<uint8_t>(ParseValue(value, 0, 255));
    } else if ((can || options.device == "usb") && arg == "--serial") {
      options.serial = value;
    } else if (can && arg == "--second-serial") {
      options.second_serial = value;
    } else if (serial && (arg == "-p" || arg == "--port")) {
      options.port = value;
    } else if (serial && arg == "--second-port") {
      options.second_port = value;
    } else if (serial && (arg == "-b" || arg == "--baudrate")) {
      options.baudrate = ParseValue(value, 1, 2147483647);
    } else if (socketcan && arg == "--can-if") {
      options.can_if = value;
    } else if (socketcan && arg == "--second-can-if") {
      options.second_can_if = value;
      second_if_set = true;
    } else if (arg == "-f" || arg == "--request-interval") {
      options.request_interval = ParseValue(value, 0, 2147483647);
    } else if (arg == "-t") {
      options.recv_timeout = ParseValue(value, 1, 2147483647);
    } else if (arg == "--position" && Contains(extras, arg)) {
      options.position = ParseValue(value, 0, 255);
    } else if (arg == "--model" && Contains(extras, arg)) {
      options.model = value;
    } else if (arg == "--revision" && Contains(extras, arg)) {
      options.revision = value;
    } else {
      throw std::invalid_argument("Unsupported option: " + std::string(arg));
    }
  }
  if (options.hand != "left" && options.hand != "right" &&
      options.hand != "both") {
    throw std::invalid_argument("Hand mode must be left, right or both");
  }
  if (index_set && !options.serial.empty()) {
    throw std::invalid_argument("Select a CAN adapter by index or serial");
  }
  if (!options.second_serial.empty() &&
      (options.hand != "both" || options.serial.empty())) {
    throw std::invalid_argument("--second-serial requires both and --serial");
  }
  if ((!options.second_port.empty() || second_if_set) &&
      options.hand != "both") {
    throw std::invalid_argument("A second port/interface requires both");
  }
  if (can && options.hand == "both" && options.second_serial.empty() &&
      options.channel == 255) {
    throw std::invalid_argument("No next CAN channel after 255");
  }
  if (serial && options.hand == "both" &&
      (options.second_port.empty() || options.second_port == options.port)) {
    throw std::invalid_argument(
        "Both hands require two different serial ports");
  }
  if (options.model != "s" && options.model != "m") {
    throw std::invalid_argument("Model must be s or m");
  }
  if (!options.revision.empty() && options.revision != "1.3" &&
      options.revision != "1.5") {
    throw std::invalid_argument("Revision must be 1.3 or 1.5");
  }
  return options;
}

inline std::shared_ptr<agilink::omnihand::DeviceContext> MakeContext(
    const Options& options, bool second = false, bool native_usb = false) {
  auto context = std::make_shared<agilink::omnihand::DeviceContext>();
  context->hand_type = second || options.hand == "right"
                           ? agilink::omnihand::HandType::RIGHT
                           : agilink::omnihand::HandType::LEFT;
  context->device_id = options.device_id;
  context->show_data_details = options.verbose;
  if (options.device == "zlgcan" || options.device == "hcan") {
    agilink::omnihand::CanConnectionInfo can;
    can.adapter =
        options.device == "hcan"
            ? agilink::omnihand::CanConnectionInfo::UsbCanAdapter::HCAN
            : agilink::omnihand::CanConnectionInfo::UsbCanAdapter::ZLG;
    can.adapter_index = options.adapter_index;
    can.channel = static_cast<uint8_t>(
        options.channel + (second && options.second_serial.empty() ? 1 : 0));
    can.serial_number = second && !options.second_serial.empty()
                            ? options.second_serial
                            : options.serial;
    context->can = std::move(can);
  } else if (options.device == "socketcan") {
    context->socket_can = second ? options.second_can_if : options.can_if;
  } else if (native_usb && options.device == "usb") {
    if (!options.port.empty() || options.baudrate != 0) {
      throw std::invalid_argument("Native USB uses --serial, not -p or -b");
    }
    context->usb = agilink::omnihand::UsbConnectionInfo{options.serial};
  } else {
    const auto& port = second ? options.second_port : options.port;
    if (port.empty() || !options.serial.empty()) {
      throw std::invalid_argument("Serial connections require -p PORT");
    }
    context->serial = agilink::omnihand::SerialConnectionInfo{
        options.device == "rs485"
            ? agilink::omnihand::SerialConnectionInfo::Mode::RS485
            : agilink::omnihand::SerialConnectionInfo::Mode::USB,
        port, options.baudrate};
  }
  return context;
}

inline void ConfigureHand(agilink::omnihand::OmniHand& hand,
                          const Options& options) {
  if (options.request_interval >= 0) {
    hand.SetRequestInterval(options.request_interval);
  }
  if (options.recv_timeout >= 0) {
    hand.SetFrameRecvTimeout(options.recv_timeout);
  }
}

}  // namespace demo

#endif  // OMNIHAND_DEMO_DEMO_OPTIONS_H_
