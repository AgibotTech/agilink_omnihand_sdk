// Copyright (c) 2026, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include "can/can_bus_device.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

namespace agilink {
namespace omnihand {
namespace {

class RecordingCanDevice : public CanBusDeviceBase {
 public:
  RecordingCanDevice() { SetMinRequestInterval(0); }
  int OpenDevice() override { return 0; }
  int CloseDevice() override { return 0; }
  void RecvFrame() override {}
  int SendFrame(unsigned int id, unsigned char* data, unsigned char size) override {
    return Record(id, data, size, true);
  }
  std::vector<CanfdFrame> sent;
  bool reply = false;

 protected:
  int SendClassicFrame(unsigned int id, unsigned char* data,
                        unsigned char size) override {
    return Record(id, data, size, false);
  }
  bool UsesSameThreadRecvPoll() const override { return true; }
  void PollIncomingSameThread() override {
    if (pending_reply_) {
      const auto frame = *pending_reply_;
      pending_reply_.reset();
      HandleReceivedFrame(frame);
    }
  }

 private:
  std::optional<CanfdFrame> pending_reply_;
  int Record(unsigned int id, const unsigned char* data, unsigned char size,
               bool is_can_fd) {
    CanfdFrame frame{};
    frame.can_id_ = id;
    frame.len_ = size;
    frame.is_can_fd_ = is_can_fd;
    std::copy_n(data, size, frame.data_);
    sent.push_back(frame);
    if (reply) {
      frame.can_id_ += 1;
      pending_reply_ = frame;
    }
    return 0;
  }
};

TEST(CanFrameTransportTest, ClassicMotorFramesDoNotChangeFdSwitchCommands) {
  RecordingCanDevice device;
  CanfdFrame toggle{};
  toggle.can_id_ = 0x07800881;
  toggle.len_ = 9;
  EXPECT_EQ(device.SendRequestWithoutReply(toggle), 0);
  for (unsigned int id : {1u, 0x601u, 21u, 0x615u}) {
    CanfdFrame motor{};
    motor.can_id_ = id;
    motor.len_ = 8;
    motor.is_can_fd_ = false;
    EXPECT_EQ(device.SendRequestWithoutReply(motor), 0);
  }
  EXPECT_EQ(device.SendRequestWithoutReply(toggle), 0);
  ASSERT_EQ(device.sent.size(), 6);
  EXPECT_TRUE(device.sent.front().is_can_fd_);
  EXPECT_TRUE(device.sent.back().is_can_fd_);
  for (size_t i = 1; i < 5; ++i) {
    EXPECT_FALSE(device.sent[i].is_can_fd_);
    EXPECT_LE(device.sent[i].can_id_, 0x7FFu);
    EXPECT_EQ(device.sent[i].len_, 8);
  }
}

TEST(CanFrameTransportTest, OversizedClassicFrameNeverReachesDriver) {
  RecordingCanDevice device;
  CanfdFrame request{};
  request.len_ = 9;
  request.is_can_fd_ = false;
  EXPECT_EQ(device.SendRequestWithoutReply(request), -1);
  EXPECT_TRUE(device.sent.empty());
}

TEST(CanFrameTransportTest, SynchronousReplyRetainsClassicFormat) {
  RecordingCanDevice device;
  device.reply = true;
  device.SetReplyMatcher({
      [](FrameView request) { return request.id + 1; },
      [](uint32_t key, FrameView response) { return response.id == key; }});
  CanfdFrame request{};
  request.can_id_ = 0x601;
  request.len_ = 8;
  request.is_can_fd_ = false;
  const auto reply = device.SendRequestSynch(request);
  EXPECT_EQ(reply.can_id_, 0x602u);
  EXPECT_FALSE(reply.is_can_fd_);
}

TEST(CanFrameTransportTest, ClassicTrafficIsIdentifiedInCommunicationLog) {
  CanfdFrame frame{};
  frame.can_id_ = 0x601;
  frame.is_can_fd_ = false;
  std::ostringstream log;
  log << frame;
  EXPECT_NE(log.str().find("[TYPE:CAN]"), std::string::npos);
}

}  // namespace
}  // namespace omnihand
}  // namespace agilink
