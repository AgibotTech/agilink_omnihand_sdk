// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "demo_options.h"
#include "omnihand/omnipicker_3.h"

namespace {

void DemoPositionControl(agilink::omnihand::OmniPicker3& hand) {
  // Motor positions use [0, 4095]; stroke control below uses 0.01 mm.
  for (const int16_t target : {1024, 2048}) {
    const auto reply = hand.SetAllJointMotorPosi({target});
    if (reply.size() != 1) {
      throw std::runtime_error("Failed to send gripper position");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto actual = hand.GetAllJointMotorPosi();
    if (actual.size() != 1) {
      throw std::runtime_error("Failed to read gripper position feedback");
    }
    std::cout << "Position target: " << target << ", actual: " << actual.front()
              << '\n';
  }
}

void OpenGripper(agilink::omnihand::OmniPicker3& hand) {
  // Open to 50 mm with a 3000 rpm speed limit and a 1000 mA current limit.
  const auto reply = hand.MixControlByStroke(5000, 3000, 1000);
  if (!reply) {
    throw std::runtime_error("No stroke control reply");
  }
  // The command reply is sampled before movement. Read again after waiting.
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const auto stroke = hand.GetStroke();
  if (!stroke) {
    throw std::runtime_error("Failed to read stroke feedback");
  }
  std::cout << "Opening target: 50 mm, actual: " << *stroke / 100.0 << " mm\n";
}

void DemoGentleGrip(agilink::omnihand::OmniPicker3& hand) {
  // Omit the argument to use the firmware's default current limit.
  const auto reply = hand.SetGentleGrip(1000);
  if (reply.empty()) {
    throw std::runtime_error("No gentle-grip control reply");
  }
  // Preserve the raw reply because its layout depends on the firmware.
  std::cout << "Gentle grip (1000 mA), reply:";
  for (const uint8_t byte : reply) {
    std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<unsigned int>(byte);
  }
  std::cout << std::dec << std::setfill(' ') << '\n';
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void ReadSensors(agilink::omnihand::OmniPicker3& hand) {
  std::cout << "Motor temperature: " << hand.GetTemperatureReport(1)
            << " C, current: " << hand.GetCurrentReport(1)
            << ", error: " << hand.GetErrorReport(1).ToString() << '\n';

  if (!hand.InitTactilePointsMap()) {
    std::cerr << "Tactile metadata unavailable\n";
    return;
  }
  for (const auto finger : hand.GetSensorOrder()) {
    const auto data = hand.GetTactileSensorDataRaw(finger);
    const size_t expected = hand.GetNumOfTactilePoints(finger);
    if (data.data_.empty() || data.data_.size() != expected) {
      std::cerr << agilink::omnihand::ToString(finger)
                << ": incomplete tactile data\n";
      continue;
    }
    std::cout << agilink::omnihand::ToString(finger) << ": "
              << data.data_.size() << " tactile points, first values:";
    for (size_t i = 0; i < std::min(size_t{8}, data.data_.size()); ++i) {
      std::cout << ' ' << data.data_[i];
    }
    std::cout << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (demo::HelpRequested(argc, argv)) {
    demo::PrintUsage(argv[0], {"zlgcan", "hcan", "socketcan", "rs485"});
    return 0;
  }
  try {
    const auto options = demo::ParseOptions(
        argc, argv, {"zlgcan", "hcan", "socketcan", "rs485"});
    if (options.hand == "both") {
      throw std::invalid_argument("OP3 demo controls one gripper per process");
    }
    auto hand =
        agilink::omnihand::OmniPicker3::CreateHand(demo::MakeContext(options));
    if (!hand) {
      throw std::runtime_error("Failed to initialize OP3 via " +
                               options.device);
    }
    demo::ConfigureHand(*hand, options);
    std::cout << hand->GetVendorInfo().ToString();

    DemoPositionControl(*hand);
    OpenGripper(*hand);
    DemoGentleGrip(*hand);
    ReadSensors(*hand);
    OpenGripper(*hand);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
