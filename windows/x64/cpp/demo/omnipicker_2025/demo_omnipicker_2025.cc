// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "agilink_logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#endif

#include "demo_options.h"
#include "omnihand/device_scanner.h"
#include "omnihand/omnipicker_2025.h"

namespace {

std::string ResolveUsbSerial(const std::string& port) {
#ifdef _WIN32
  const auto raw_devices = SetupDiGetClassDevsA(
      nullptr, "USB", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
  if (raw_devices == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to enumerate USB ports");
  }
  std::unique_ptr<void, decltype(&SetupDiDestroyDeviceInfoList)> devices(
      raw_devices, SetupDiDestroyDeviceInfoList);
  const auto port_name = port.compare(0, 4, "\\\\.\\") == 0 ? port.substr(4) : port;
  SP_DEVINFO_DATA info{};
  info.cbSize = sizeof(info);
  for (DWORD index = 0;
       SetupDiEnumDeviceInfo(devices.get(), index, &info); ++index) {
    const HKEY key = SetupDiOpenDevRegKey(
        devices.get(), &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
    if (key == INVALID_HANDLE_VALUE) {
      continue;
    }
    char name[256]{};
    DWORD size = sizeof(name);
    DWORD type = 0;
    const auto result = RegQueryValueExA(
        key, "PortName", nullptr, &type, reinterpret_cast<BYTE*>(name), &size);
    RegCloseKey(key);
    name[sizeof(name) - 1] = '\0';
    if (result != ERROR_SUCCESS || type != REG_SZ ||
        _stricmp(name, port_name.c_str()) != 0) {
      continue;
    }
    // A composite USB device's COM interface has a generated instance ID.
    // Walk to its physical parent to obtain the descriptor serial number.
    DEVINST node = info.DevInst;
    do {
      char instance_id[MAX_DEVICE_ID_LEN]{};
      if (CM_Get_Device_IDA(node, instance_id, sizeof(instance_id), 0) !=
          CR_SUCCESS) {
        break;
      }
      constexpr char kUsbPrefix[] = "USB\\VID_1001&PID_03EF\\";
      if (_strnicmp(instance_id, kUsbPrefix, sizeof(kUsbPrefix) - 1) == 0) {
        return instance_id + sizeof(kUsbPrefix) - 1;
      }
      DEVINST parent = 0;
      if (CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) {
        break;
      }
      node = parent;
    } while (true);
  }
#else
  for (const auto& device : agilink::omnihand::ScanSerialDevices()) {
    if (device.port != port) {
      continue;
    }
    constexpr char kUsbId[] = "USB VID:PID=1001:03ef ";
    if (device.hardware_id.compare(0, sizeof(kUsbId) - 1, kUsbId) != 0) {
      break;
    }
    const auto start = device.hardware_id.find("SNR=");
    if (start == std::string::npos) {
      break;
    }
    const auto serial = device.hardware_id.substr(start + 4);
    if (!serial.empty()) {
      return serial;
    }
  }
#endif
  throw std::invalid_argument("No OP1 USB port matches " +
                              port + "; use --serial USB_SERIAL if needed");
}

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
        "USB: -p PORT selects by COM/tty port, or use --serial USB_SERIAL.\n"
        "Omit both to select the first USB device. USB has no baud rate.\n"
        "  --position N       RS485 target [0,255]; omit to read info only\n"
        "RS485 defaults to 115200 baud, 8N1. V1.2: DIP 1/2 ON, 3-6 OFF.\n"
        "Firmware: rs485_mode=1, modbus_mode=0; save and reboot.\n");
    return 0;
  }
  try {
    auto options = demo::ParseOptions(
        argc, argv, {"usb", "zlgcan", "hcan", "socketcan", "rs485"},
        {"--position"});
    if (options.hand == "both") {
      throw std::invalid_argument("OP1 demo controls one gripper per process");
    }
    if (options.position >= 0 && options.device != "rs485") {
      throw std::invalid_argument("--position needs RS485");
    }
    if (options.device == "usb" && !options.port.empty()) {
      if (!options.serial.empty() || options.baudrate != 0) {
        throw std::invalid_argument(
            "USB -p PORT cannot be combined with --serial or -b");
      }
      options.serial = ResolveUsbSerial(options.port);
      options.port.clear();
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
    agilink::AgilinkLogger::get().flush();
    std::cerr << error.what() << '\n';
    return 1;
  }
}
