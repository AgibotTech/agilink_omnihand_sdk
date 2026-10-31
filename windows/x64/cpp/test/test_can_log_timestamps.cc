// Copyright (c) 2026, Agibot Co., Ltd.
// OmniHand SDK is licensed under Mulan PSL v2.

#include <gtest/gtest.h>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "agilink_logger.h"
#include "can/can_bus_device.h"
#include "utils/hex_format.h"

using namespace agilink;
using namespace agilink::omnihand;

namespace {
using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;

Clock::time_point ParseLogTime(const std::string& line) {
  std::tm local{};
  int year, month, microseconds;
  if (std::sscanf(line.c_str(), "[%d-%d-%d %d:%d:%d.%d]", &year, &month,
                  &local.tm_mday, &local.tm_hour, &local.tm_min,
                  &local.tm_sec, &microseconds) != 7) {
    ADD_FAILURE() << "Missing timestamp: " << line;
    return {};
  }
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_isdst = -1;
  return Clock::from_time_t(std::mktime(&local)) + std::chrono::microseconds(microseconds);
}

thread_local Clock::time_point match_start;

uint32_t ReplyKey(FrameView request) { return request.id; }
bool MatchReply(uint32_t key, FrameView reply) {
  match_start = Clock::now();
  // Make cache processing long enough to expose a timestamp taken after matching.
  std::this_thread::sleep_for(10ms);
  return key == reply.id;
}

class FakeCanDevice : public CanBusDeviceBase {
 public:
  FakeCanDevice() {
    SetSerializedRequestQueue(false);
    SetReplyMatcher({ReplyKey, MatchReply});
    SetFrameRecvTimeout(100);
    device_label_ = "[Fake CAN]";
  }
  int OpenDevice() override { return 0; }
  int CloseDevice() override { return 0; }
  void RecvFrame() override {}
  int SendFrame(unsigned int id, unsigned char*, unsigned char) override {
    send_enter = Clock::now();
    std::this_thread::sleep_for(10ms);
    replies.clear();
    for (uint8_t index = 0; index < reply_count; ++index) {
      CanfdFrame reply;
      reply.can_id_ = id;
      reply.len_ = 2;
      reply.data_[0] = reply_count;
      reply.data_[1] = index;
      replies.push_back(reply);
    }
    send_return = Clock::now();
    return send_result;
  }
  uint8_t reply_count = 1;
  int send_result = 0;
  Clock::time_point send_enter;
  Clock::time_point send_return;

 protected:
  bool UsesSameThreadRecvPoll() const override { return true; }
  void PollIncomingSameThread() override {
    auto pending = std::move(replies);
    replies.clear();
    for (const auto& reply : pending) HandleReceivedFrame(reply);
  }

 private:
  std::vector<CanfdFrame> replies;
};

CanfdFrame Request() {
  CanfdFrame request;
  request.can_id_ = 0x00130801;
  return request;
}

void CheckSendTime(const std::string& line, const FakeCanDevice& device) {
  const auto timestamp = ParseLogTime(line);
  EXPECT_LE(timestamp, device.send_enter);
  EXPECT_GE(timestamp, device.send_enter - 5ms);
  EXPECT_GE(device.send_return - timestamp, 10ms);
}

TEST(CanLogTimestamps, CallbackUsesSendEntryAndPreCacheReceiveTime) {
  FakeCanDevice device;
  std::vector<std::string> lines;
  Clock::time_point send_log_call;
  device.ShowDataDetails(true, [&](const std::string& line) {
    lines.push_back(line);
    if (line.find("SND:") != std::string::npos) send_log_call = Clock::now();
  });
  device.SendRequestSynch(Request());
  ASSERT_EQ(lines.size(), 2u);
  CheckSendTime(lines[0], device);
  EXPECT_GE(send_log_call, device.send_return);
  EXPECT_LE(ParseLogTime(lines[1]), match_start);
  EXPECT_GE(Clock::now() - ParseLogTime(lines[1]), 10ms);
}

TEST(CanLogTimestamps, MultiFrameAndNoReplyKeepSendTime) {
  FakeCanDevice device;
  std::vector<std::string> lines;
  device.ShowDataDetails(true, [&](const std::string& line) { lines.push_back(line); });
  device.reply_count = 2;
  const auto replies = device.SendRequestSynchMultiFrame(Request(), 2);
  ASSERT_EQ(replies.size(), 2u);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_NE(lines[0].find("SND (MultiFrame):"), std::string::npos);
  CheckSendTime(lines[0], device);

  lines.clear();
  device.reply_count = 0;
  EXPECT_EQ(device.SendRequestWithoutReply(Request()), 0);
  ASSERT_EQ(lines.size(), 1u);
  CheckSendTime(lines[0], device);

  lines.clear();
  device.send_result = -1;
  EXPECT_EQ(device.SendRequestWithoutReply(Request()), -1);
  EXPECT_TRUE(lines.empty());
  EXPECT_THROW(device.SendRequestSynch(Request()), std::runtime_error);
  EXPECT_TRUE(lines.empty());
}

TEST(CanLogTimestamps, AgilinkLoggerUsesCapturedSendAndReceiveTime) {
  std::mutex mutex;
  std::vector<std::string> lines;
  AgilinkLogger::init([&](OmniLogger<>::Level, const char* text, size_t length) {
    std::lock_guard<std::mutex> lock(mutex);
    lines.emplace_back(text, length);
  }, nullptr, OmniLogger<>::Level::VERBOSE, 1ms, true);
  FakeCanDevice device;
  device.ShowDataDetails(true);
  device.SendRequestSynch(Request());
  AgilinkLogger::get().flush();
  // Destroy the logger worker while its captured sink is still alive.
  AgilinkLogger::init();
  {
    std::lock_guard<std::mutex> lock(mutex);
    ASSERT_EQ(lines.size(), 2u);
    CheckSendTime(lines[0], device);
    EXPECT_NE(lines[0].find("[V] [Fake CAN] SND:"), std::string::npos);
    EXPECT_LE(ParseLogTime(lines[1]), match_start);
    EXPECT_NE(lines[1].find("[V] [Fake CAN] RCV:"), std::string::npos);
  }
}

TEST(CanLogTimestamps, ExplicitTimePreservesLoggerFilteringAndTimestampOption) {
  std::vector<std::string> lines;
  OmniLogger<> logger([&](OmniLogger<>::Level, const char* text, size_t length) {
    lines.emplace_back(text, length);
  }, nullptr, OmniLogger<>::Level::WARN, 1ms, false);
  const auto event_time = Clock::now() - 1h;
  logger.log(OmniLogger<>::Level::INFO, "filtered", event_time);
  logger.log(OmniLogger<>::Level::WARN, "warning", event_time);
  logger.flush();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0], "[W] warning");
}

TEST(CanLogTimestamps, ExplicitTimestampIsFormattedExactly) {
  std::vector<std::string> lines;
  OmniLogger<> logger([&](OmniLogger<>::Level, const char* text, size_t length) {
    lines.emplace_back(text, length);
  }, nullptr, OmniLogger<>::Level::VERBOSE, 1ms, true);
  const auto event_time = Clock::time_point{std::chrono::microseconds{1700000000123456}};
  logger.log(OmniLogger<>::Level::INFO, "event", event_time);
  logger.flush();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0], FormatLogTimestamp(event_time) + " [I] event");
}
}  // namespace
