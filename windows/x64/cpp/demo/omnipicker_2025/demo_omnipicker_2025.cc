// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "demo_options.h"
#include "omnihand/omnipicker_2025.h"

namespace {

void RunUsb(agilink::omnihand::OmniPicker2025& picker,
            const agilink::omnihand::Op1DeviceInfo& info) {
  if (!info.motor.calib_valid || info.motor.error_code != 0) {
    throw std::runtime_error(
        "USB motion requires valid calibration and no motor errors");
  }

  // The public gesture API sends a MIT target over native USB.
  // USB returns the commanded ratio; it does not report measured position.
  picker.SetHandGesture(agilink::omnihand::OmniPicker2025Gesture::
                            OMNIPICKER_2025_GESTURE_HALF_OPEN);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "USB half-open target sent (50% opening).\n";
}

void RunCan(agilink::omnihand::OmniPicker2025& picker) {
  // Single-node control: position [0,255], followed by
  // speed/force/acceleration/deceleration.
  const agilink::omnihand::Op1CanfdCtrlFrame target{128, 255, 255, 255, 255};
  const auto reply = picker.SendFrameSyncByCanfd(target);
  if (!reply) {
    throw std::runtime_error("No CAN control reply");
  }
  // Feedback is sampled when the command arrives, before motion completes.
  std::cout << agilink::omnihand::ToString(*reply) << '\n';
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

}  // namespace

int main(int argc, char** argv) {
  if (demo::HelpRequested(argc, argv)) {
    demo::PrintUsage(
        argv[0], {"usb", "zlgcan", "hcan", "socketcan", "rs485"},
        "Native USB: --serial USB_SERIAL selects a device; omit for first.\n"
        "  --position N       RS485 target [0,255]; omit to read info only\n"
        "RS485 defaults to 115200 baud, 8N1. V1.2: DIP 1/2 ON, 3-6 OFF.\n"
        "Firmware: rs485_mode=1, modbus_mode=0; save and reboot.\n");
    return 0;
  }
  try {
    const auto options = demo::ParseOptions(
        argc, argv, {"usb", "zlgcan", "hcan", "socketcan", "rs485"},
        {"--position"});
    if (options.hand == "both") {
      throw std::invalid_argument("OP1 demo controls one gripper per process");
    }
    if (options.position >= 0 && options.device != "rs485") {
      throw std::invalid_argument("--position needs RS485");
    }
    auto picker = agilink::omnihand::OmniPicker2025::CreateHand(
        demo::MakeContext(options, false, true));
    if (!picker) {
      throw std::runtime_error("Failed to initialize OP1 via " +
                               options.device);
    }
    demo::ConfigureHand(*picker, options);
    const auto info = picker->ShowDeviceInfo();
    std::cout << agilink::omnihand::ToString(info) << '\n';
    if (options.device == "usb") {
      RunUsb(*picker, info);
    } else if (options.device == "rs485") {
      if (options.position >= 0) {
        const auto reply = picker->SendFrameSyncByRs485(
            {static_cast<uint8_t>(options.position), 1, 1, 1, 1});
        if (!reply) {
          throw std::runtime_error("No RS485 control reply");
        }
        std::cout << agilink::omnihand::ToString(*reply) << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    } else {
      RunCan(*picker);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
