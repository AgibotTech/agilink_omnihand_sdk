// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "demo_options.h"
#include "omnihand/omnihand_2025.h"

namespace {

std::unique_ptr<agilink::omnihand::OmniHand2025> CreateHand(
    const demo::Options& options, bool second = false) {
  auto context = demo::MakeContext(options, second);
  // CreateHand initializes the connection; no separate Init() is needed.
  auto hand = agilink::omnihand::OmniHand2025::CreateHand(std::move(context));
  if (!hand) {
    throw std::runtime_error("Failed to initialize OmniHand2025 via " +
                             options.device);
  }
  demo::ConfigureHand(*hand, options);
  return hand;
}

void ReadSensors(agilink::omnihand::OmniHand2025& hand) {
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

  // Read downsampled tactile data for all sensors in one call.
  const auto tactile = hand.GetAllTactileSensorData();
  std::cout << "Downsampled tactile data:\n";
  for (const auto& sensor : tactile) {
    std::cout << agilink::omnihand::ToString(sensor.sensor_id_) << ':';
    for (const auto value : sensor.data_) {
      std::cout << ' ' << +value;
    }
    std::cout << '\n';
  }

  // Raw data retains full resolution and requires firmware/transport support.
  // Unsupported reads are reported by the SDK.
  const auto raw_tactile = hand.GetAllTactileSensorDataRaw();
  std::cout << "Raw tactile data:\n";
  for (const auto& sensor : raw_tactile) {
    std::cout << agilink::omnihand::ToString(sensor.sensor_id_) << ':';
    for (const auto value : sensor.data_) {
      std::cout << ' ' << +value;
    }
    std::cout << '\n';
  }
}

void RunDemo(agilink::omnihand::OmniHand2025& hand, const char* name) {
  std::cout << name << " hand:\n" << hand.GetVendorInfo().ToString();

  // Preserve the other joints at their measured positions.
  auto angles = hand.GetAllActiveJointAngles();
  if (angles.size() != hand.GetDoA()) {
    throw std::runtime_error("Failed to read the initial joint angles");
  }
  constexpr size_t kJoint = agilink::omnihand::o10::ActiveJointIndexPIP;
  // index PIP flexion in radians; both targets are within the joint limits.
  for (const double target : {0.35, 0.7}) {
    angles[kJoint] = target;
    const auto reply = hand.SetAllActiveJointAngles(angles);
    if (reply.size() != angles.size()) {
      throw std::runtime_error("Failed to send joint angles");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto actual = hand.GetAllActiveJointAngles();
    if (actual.size() != angles.size()) {
      throw std::runtime_error("Failed to read joint angle feedback");
    }
    std::cout << "index PIP target: " << target
              << " rad, actual: " << actual[kJoint] << " rad\n";
  }
  ReadSensors(hand);
}

}  // namespace

int main(int argc, char** argv) {
  if (demo::HelpRequested(argc, argv)) {
    demo::PrintUsage(argv[0], {"zlgcan", "hcan", "socketcan", "usb", "rs485"});
    return 0;
  }
  try {
    const auto options = demo::ParseOptions(
        argc, argv, {"zlgcan", "hcan", "socketcan", "usb", "rs485"});
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
