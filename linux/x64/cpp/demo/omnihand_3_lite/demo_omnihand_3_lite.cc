// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "demo_options.h"
#include "omnihand/omnihand_3_lite.h"

namespace {

std::unique_ptr<agilink::omnihand::OmniHand3Lite> CreateHand(
    const demo::Options& options, bool second = false) {
  auto context = demo::MakeContext(options, second);
  context->h3l_model = options.model == "m"
                           ? agilink::omnihand::h3l::H3LModel::H3L_M
                           : agilink::omnihand::h3l::H3LModel::H3L_S;
  auto hand = agilink::omnihand::OmniHand3Lite::CreateHand(std::move(context));
  if (!hand) {
    throw std::runtime_error("Failed to initialize H3L via " + options.device);
  }
  demo::ConfigureHand(*hand, options);
  return hand;
}

void ReadSensors(agilink::omnihand::OmniHand3Lite& hand) {
  std::cout << "Motor temperatures (C):";
  for (const auto value : hand.GetAllTemperatureReport()) {
    std::cout << ' ' << +value;
  }
  std::cout << "\nMotor currents (protocol units):";
  for (const auto value : hand.GetAllCurrentReport()) {
    std::cout << ' ' << +value;
  }
  std::cout << '\n';
  const auto errors = hand.GetAllErrorReport();
  for (size_t i = 0; i < errors.size(); ++i) {
    std::cout << "Motor " << i + 1 << " error: " << errors[i].ToString()
              << '\n';
  }

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
      std::cout << ' ' << +data.data_[i];
    }
    std::cout << '\n';
  }
}

void RunDemo(agilink::omnihand::OmniHand3Lite& hand, const char* name) {
  std::cout << name << " hand:\n" << hand.GetVendorInfo().ToString();

  // Built-in gestures adapt to the selected model and hand side.
  for (const auto gesture : {agilink::omnihand::h3l::OmniHand3LiteGesture::
                                 OMNI_HAND_3_LITE_GESTURE_PRAYER,
                             agilink::omnihand::h3l::OmniHand3LiteGesture::
                                 OMNI_HAND_3_LITE_GESTURE_SIDE_FIST}) {
    if (hand.SetHandGesture(gesture).size() != hand.GetDoA()) {
      throw std::runtime_error("Failed to send gesture");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto angles = hand.GetAllActiveJointAngles();
    if (angles.size() != hand.GetDoA()) {
      throw std::runtime_error("Failed to read joint angle feedback");
    }
    std::cout << "Active joint angles (rad):";
    for (const double angle : angles) {
      std::cout << ' ' << angle;
    }
    std::cout << '\n';
  }
  ReadSensors(hand);
}

}  // namespace

int main(int argc, char** argv) {
  if (demo::HelpRequested(argc, argv)) {
    demo::PrintUsage(argv[0], {"zlgcan", "hcan", "socketcan", "rs485"},
                     "  --model s|m        H3L model (default: s)\n");
    return 0;
  }
  try {
    const auto options = demo::ParseOptions(
        argc, argv, {"zlgcan", "hcan", "socketcan", "rs485"}, {"--model"});
    auto hand = CreateHand(options);
    if (options.hand == "both") {
      auto right = CreateHand(options, true);
      RunDemo(*hand, "Left");
      RunDemo(*right, "Right");
    } else {
      RunDemo(*hand, options.hand.c_str());
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
