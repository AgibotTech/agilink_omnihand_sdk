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
#include "omnihand/omnihand_3_ultra_m.h"

namespace {

std::unique_ptr<agilink::omnihand::OmniHand3UltraM> CreateHand(
    const demo::Options& options, bool second = false) {
  auto context = demo::MakeContext(options, second);
  // CreateHand initializes the connection; no separate Init() is needed.
  auto hand =
      agilink::omnihand::OmniHand3UltraM::CreateHand(std::move(context));
  if (!hand) {
    throw std::runtime_error("Failed to initialize OmniHand3UltraM via " +
                             options.device);
  }
  demo::ConfigureHand(*hand, options);
  return hand;
}

void ReadSensors(agilink::omnihand::OmniHand3UltraM& hand) {
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
    std::cout << "Motor " << i + 1 << " error: "
              << agilink::omnihand::H3UMErrorReportToString(errors[i]) << '\n';
  }

  if (hand.GetRevision() == agilink::omnihand::h3um::H3UMRevision::T1P5) {
    std::cout << "Palm board: " << hand.GetTemperatureOfPalm()
              << " C, power board: " << hand.GetTemperatureOfPower() << " C\n";
  }
  if (!hand.InitTactilePointsMap()) {
    std::cerr << "Tactile metadata unavailable\n";
    return;
  }
  // Query through the interface supported by the detected hardware revision.
  if (auto* tactile = hand.GetTactileSensor3D()) {
    for (const auto finger : tactile->GetSensorOrder()) {
      const auto data = tactile->GetTactileSensorDataRaw(finger);
      if (data.points.empty() ||
          data.points.size() != tactile->GetNumOfTactilePoints(finger)) {
        std::cerr << agilink::omnihand::ToString(finger)
                  << ": incomplete 3D tactile data\n";
        continue;
      }
      const auto force = tactile->GetTactileSensorResultantForce(finger);
      const auto& point = data.points.front();
      std::cout << agilink::omnihand::ToString(finger) << ": "
                << data.points.size() << " 3D points, first XYZ: " << point.x
                << ' ' << point.y << ' ' << point.z
                << ", resultant XYZ: " << force.force.x << ' ' << force.force.y
                << ' ' << force.force.z << '\n';
    }
  } else if (auto* tactile = hand.GetTactileSensor1D()) {
    for (const auto finger : tactile->GetSensorOrder()) {
      const auto data = tactile->GetTactileSensorDataRaw(finger);
      if (data.data_.empty() ||
          data.data_.size() != tactile->GetNumOfTactilePoints(finger)) {
        std::cerr << agilink::omnihand::ToString(finger)
                  << ": incomplete pressure data\n";
        continue;
      }
      std::cout << agilink::omnihand::ToString(finger) << ": "
                << data.data_.size() << " pressure points, first values:";
      for (size_t i = 0; i < std::min(size_t{8}, data.data_.size()); ++i) {
        std::cout << ' ' << data.data_[i];
      }
      std::cout << '\n';
    }
  }
}

void RunDemo(agilink::omnihand::OmniHand3UltraM& hand, const char* name) {
  std::cout << name << " hand:\n" << hand.GetVendorInfo().ToString();

  // Preserve the other joints at their measured positions.
  auto angles = hand.GetAllActiveJointAngles();
  if (angles.size() != hand.GetDoA()) {
    throw std::runtime_error("Failed to read the initial joint angles");
  }
  constexpr size_t kJoint = static_cast<size_t>(
      agilink::omnihand::h3um::H3UMActiveJoint::H3U_M_ACTIVE_JOINT_PINKY_PIP);
  // pinky PIP flexion in radians; both targets are within the joint limits.
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
    std::cout << "pinky PIP target: " << target
              << " rad, actual: " << actual[kJoint] << " rad\n";
  }
  ReadSensors(hand);
}

#ifdef OMNIHAND_WITH_XENSE
void RunXense(agilink::omnihand::OmniHand3UltraM& hand) {
  // Collect one finger for five seconds without commanding motion.
  agilink::omnihand::XenseFrame frame;
  for (int i = 0; i < 50; ++i) {
    if (hand.GetFingerTactile(0, frame)) {
      std::cout << "Thumb image: " << frame.rectify_width << 'x'
                << frame.rectify_height << ", timestamp: " << frame.timestamp
                << " ns\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
#endif

}  // namespace

int main(int argc, char** argv) {
  if (demo::HelpRequested(argc, argv)) {
    demo::PrintUsage(
        argv[0], {"zlgcan", "hcan", "socketcan"},
        "  Device ID defaults to 0: discover address and detect T1.3/T1.5.\n"
        "  --xense           Read visual tactile data instead of motion\n", 0);
    return 0;
  }
  try {
    const auto options =
        demo::ParseOptions(argc, argv, {"zlgcan", "hcan", "socketcan"},
                           {"--xense"}, 0);
    if (options.xense && options.hand == "both") {
      throw std::invalid_argument("Xense demo reads one hand per process");
    }
#ifndef OMNIHAND_WITH_XENSE
    if (options.xense) {
      throw std::invalid_argument("This SDK was built without Xense support");
    }
#endif
    auto hand = CreateHand(options);
#ifdef OMNIHAND_WITH_XENSE
    if (options.xense) {
      RunXense(*hand);
      return 0;
    }
#endif
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
