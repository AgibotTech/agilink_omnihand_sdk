// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include "can/zlg_usbcanfd_device.h"
#include "omnihand/impl/eyou_motor_ota_channel.h"
#include "zlgcan.h"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

namespace {
std::mutex driver_mutex;
std::condition_variable driver_cv;
std::deque<ZCAN_Receive_Data> classic_rx;
std::deque<ZCAN_ReceiveFD_Data> fd_rx;
std::vector<ZCAN_Transmit_Data> classic_tx;
std::vector<ZCAN_TransmitFD_Data> fd_tx;
bool fail_classic_send = false;
}

// In-memory vendor API: this executable never links or opens the real driver.
extern "C" {
DEVICE_HANDLE FUNC_CALL ZCAN_OpenDevice(UINT, UINT index, UINT) {
  return index == 0 ? reinterpret_cast<DEVICE_HANDLE>(1) : nullptr;
}
UINT FUNC_CALL ZCAN_CloseDevice(DEVICE_HANDLE) { return STATUS_OK; }
UINT FUNC_CALL ZCAN_GetDeviceInf(DEVICE_HANDLE, ZCAN_DEVICE_INFO* info) {
  std::memset(info, 0, sizeof(*info));
  info->can_Num = 1;
  std::memcpy(info->str_Serial_Num, "FAKE_CLASSIC_CAN", 16);
  std::memcpy(info->str_hw_Type, "USBCANFD-200U", 14);
  return STATUS_OK;
}
CHANNEL_HANDLE FUNC_CALL ZCAN_InitCAN(DEVICE_HANDLE, UINT,
                                      ZCAN_CHANNEL_INIT_CONFIG*) {
  return reinterpret_cast<CHANNEL_HANDLE>(2);
}
UINT FUNC_CALL ZCAN_StartCAN(CHANNEL_HANDLE) { return STATUS_OK; }
UINT FUNC_CALL ZCAN_ResetCAN(CHANNEL_HANDLE) { return STATUS_OK; }
UINT FUNC_CALL ZCAN_SetValue(DEVICE_HANDLE, const char*, const void*) {
  return STATUS_OK;
}
UINT FUNC_CALL ZCAN_Transmit(CHANNEL_HANDLE, ZCAN_Transmit_Data* messages, UINT size) {
  std::lock_guard<std::mutex> lock(driver_mutex);
  if (fail_classic_send) {
    return 0;
  }
  classic_tx.insert(classic_tx.end(), messages, messages + size);
  for (UINT i = 0; i < size; ++i) {
    const auto& request = messages[i].frame;
    ZCAN_Receive_Data reply{};
    reply.timestamp = 1000;
    reply.frame.can_dlc = 8;
    if (request.can_id == 4) {
      reply.frame.can_id = 0xFF;
      const uint8_t data[]{0xFE, 6, 0xF2, 0x55, 0, 0, 0, 1};
      std::copy_n(data, 8, reply.frame.data);
    } else if (request.can_id == 0x601) {
      reply.frame.can_id = 0x581;
      const uint8_t data[]{0x41, 0x0A, 0x10, 0, 0x0D, 0, 0, 0};
      std::copy_n(data, 8, reply.frame.data);
    } else {
      continue;
    }
    classic_rx.push_back(reply);
  }
  return size;
}
UINT FUNC_CALL ZCAN_TransmitFD(CHANNEL_HANDLE, ZCAN_TransmitFD_Data* messages,
                               UINT size) {
  std::lock_guard<std::mutex> lock(driver_mutex);
  fd_tx.insert(fd_tx.end(), messages, messages + size);
  ZCAN_ReceiveFD_Data ack{};
  ack.timestamp = 1000;
  ack.frame.can_id = 0x07800801 | CAN_EFF_FLAG;
  ack.frame.len = 1;
  ack.frame.data[0] = 1;
  fd_rx.push_back(ack);
  driver_cv.notify_all();
  return size;
}
UINT FUNC_CALL ZCAN_Receive(CHANNEL_HANDLE, ZCAN_Receive_Data* messages, UINT size,
                             int wait_time) {
  EXPECT_EQ(wait_time, 0);
  std::lock_guard<std::mutex> lock(driver_mutex);
  const auto count = static_cast<UINT>(std::min<size_t>(size, classic_rx.size()));
  for (UINT i = 0; i < count; ++i) {
    messages[i] = classic_rx.front();
    classic_rx.pop_front();
  }
  return count;
}
UINT FUNC_CALL ZCAN_ReceiveFD(CHANNEL_HANDLE, ZCAN_ReceiveFD_Data* messages,
                               UINT size, int wait_time) {
  std::unique_lock<std::mutex> lock(driver_mutex);
  driver_cv.wait_for(lock, std::chrono::milliseconds(wait_time),
                     [] { return !fd_rx.empty(); });
  const auto count = static_cast<UINT>(std::min<size_t>(size, fd_rx.size()));
  for (UINT i = 0; i < count; ++i) {
    messages[i] = fd_rx.front();
    fd_rx.pop_front();
  }
  return count;
}
}

namespace agilink {
namespace omnihand {
namespace {

class ZlgClassicCanTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    {
      std::lock_guard<std::mutex> lock(driver_mutex);
      classic_rx.clear();
      fd_rx.clear();
      classic_tx.clear();
      fd_tx.clear();
      fail_classic_send = false;
    }
    device_ = std::make_unique<ZlgUsbcanfdDevice>();
    ASSERT_TRUE(device_->IsInit());
    device_->SetMinRequestInterval(0);
  }
  void TearDown() override { device_.reset(); }
  std::unique_ptr<ZlgUsbcanfdDevice> device_;
};

TEST_F(ZlgClassicCanTransportTest, ScreenshotCanopenReplyIsReadFromClassicQueue) {
  EyouMotorOtaChannel channel([&](const EyouMotorOtaFrame& motor) {
    CanfdFrame frame{};
    frame.can_id_ = motor.can_id;
    frame.len_ = motor.size;
    frame.is_can_fd_ = false;
    std::copy_n(motor.data.begin(), 8, frame.data_);
    return device_->SendRequestWithoutReply(frame) == 0;
  });
  device_->SetCallback([&](const CanfdFrame& frame) {
    EXPECT_FALSE(frame.is_can_fd_);
    EyouMotorOtaFrame reply{};
    reply.can_id = static_cast<uint16_t>(frame.can_id_);
    reply.size = frame.len_;
    std::copy_n(frame.data_, 8, reply.data.begin());
    channel.OnFrame(reply);
  });
  const auto reply = channel.Exchange(EyouMotorOtaProtocol(1).QueryCanopenVersion(),
                                       0x581, 500);
  ASSERT_TRUE(reply);
  EXPECT_EQ(reply->data, (std::array<uint8_t, 8>{0x41, 0x0A, 0x10, 0, 13, 0, 0, 0}));
  std::lock_guard<std::mutex> lock(driver_mutex);
  ASSERT_EQ(classic_tx.size(), 1);
  EXPECT_EQ(classic_tx[0].frame.can_id, 0x601u);
  EXPECT_EQ(classic_tx[0].frame.can_dlc, 8);
  EXPECT_EQ(classic_tx[0].transmit_type, 0u);
  EXPECT_TRUE(fd_tx.empty());
}

TEST_F(ZlgClassicCanTransportTest, BootQueryAndFdSwitchesUseSeparateDriverApis) {
  CanfdFrame toggle{};
  toggle.can_id_ = 0x07800881;
  toggle.len_ = 9;
  EXPECT_EQ(device_->SendRequestWithoutReply(toggle), 0);
  CanfdFrame boot{};
  boot.can_id_ = 4;
  boot.len_ = 8;
  boot.is_can_fd_ = false;
  EXPECT_EQ(device_->SendRequestWithoutReply(boot), 0);
  EXPECT_EQ(device_->SendRequestWithoutReply(toggle), 0);
  std::lock_guard<std::mutex> lock(driver_mutex);
  ASSERT_EQ(classic_tx.size(), 1);
  EXPECT_EQ(classic_tx[0].frame.can_id, 4u);
  ASSERT_EQ(fd_tx.size(), 2);
  for (const auto& frame : fd_tx) {
    EXPECT_TRUE(IS_EFF(frame.frame.can_id));
    EXPECT_EQ(frame.frame.flags, CANFD_BRS);
    EXPECT_EQ(frame.frame.len, 9);
  }
}

TEST_F(ZlgClassicCanTransportTest, ClassicDriverFailureIsReturnedToCaller) {
  {
    std::lock_guard<std::mutex> lock(driver_mutex);
    fail_classic_send = true;
  }
  CanfdFrame frame{};
  frame.can_id_ = 0x601;
  frame.len_ = 8;
  frame.is_can_fd_ = false;
  EXPECT_EQ(device_->SendRequestWithoutReply(frame), -1);
  std::lock_guard<std::mutex> lock(driver_mutex);
  EXPECT_TRUE(fd_tx.empty());
}

}  // namespace
}  // namespace omnihand
}  // namespace agilink
