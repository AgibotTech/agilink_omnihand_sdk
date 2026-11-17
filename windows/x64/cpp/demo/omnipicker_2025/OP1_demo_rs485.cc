// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <charconv>
#include <iostream>
#include <string_view>

#include "omnihand/omnipicker_2025.h"

namespace {

bool ParseValue(std::string_view text, int minimum, int maximum, int& value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
         value >= minimum && value <= maximum;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 4) {
    std::cerr << "Usage: " << argv[0] << " PORT NODE_ID [POSITION_0_TO_255]\n";
    return 1;
  }
  int node_id = 0;
  int position = 0;
  if (!ParseValue(argv[2], 1, 127, node_id) ||
      (argc == 4 && !ParseValue(argv[3], 0, 255, position))) {
    std::cerr << "Invalid node ID or position\n";
    return 1;
  }
  using namespace agilink::omnihand;
  auto picker = OmniPicker2025::createHandByRs485(
      HandType::LEFT, static_cast<uint8_t>(node_id), argv[1]);
  if (!picker) {
    std::cerr << "No OP1 response on the selected port and node ID\n";
    return 1;
  }
  std::cout << ToString(picker->ShowDeviceInfo()) << '\n';
  if (argc == 4) {
    const auto reply = picker->SendFrameSyncByRs485(
        {static_cast<uint8_t>(position), 1, 1, 1, 1});
    if (!reply) {
      std::cerr << "RS485 control failed\n";
      return 1;
    }
    std::cout << ToString(*reply) << '\n';
  }
  return 0;
}
