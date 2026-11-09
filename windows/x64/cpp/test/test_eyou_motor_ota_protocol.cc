// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include "omnihand/impl/eyou_motor_ota_protocol.h"
#include "omnihand/impl/eyou_motor_ota_channel.h"

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace agilink {
namespace omnihand {
namespace {

using Payload = std::array<uint8_t, 8>;

struct ScriptedExchange {
  std::deque<std::optional<EyouMotorOtaFrame>> replies;
  std::vector<EyouMotorOtaFrame> requests;
  std::vector<uint16_t> reply_ids;

  std::optional<EyouMotorOtaFrame> operator()(
      const EyouMotorOtaFrame& request, uint16_t reply_id, int timeout_ms) {
    requests.push_back(request);
    reply_ids.push_back(reply_id);
    EXPECT_GT(timeout_ms, 0);
    if (replies.empty()) {
      ADD_FAILURE() << "Unexpected request";
      return std::nullopt;
    }
    const auto reply = replies.front();
    replies.pop_front();
    return reply;
  }
};

TEST(EyouMotorOtaProtocolTest, PcSideIdsRetainGlobalJointNumberAcrossBuses) {
  for (const uint8_t node_id : {1, 8, 9, 16, 17, 21}) {
    const EyouMotorOtaProtocol protocol(node_id);
    EXPECT_EQ(protocol.CheckBootNode().can_id, node_id);
    EXPECT_EQ(protocol.EnterBootFromApp().can_id, 0x600 + node_id);
    EXPECT_EQ(protocol.QueryCanopenVersion().can_id, 0x600 + node_id);
    EXPECT_EQ(protocol.QueryCanopenVersionSegment(false).can_id, 0x600 + node_id);
    EXPECT_EQ(protocol.GetCanopenReplyCanId(), 0x580 + node_id);
  }
}

TEST(EyouMotorOtaProtocolTest, BootQueryTypeDeterminesBootOrAppWithoutFallback) {
  const EyouMotorOtaProtocol protocol(9);
  for (const uint8_t type : {0x55, 0xAA}) {
    ScriptedExchange exchange{{EyouMotorOtaFrame{
        0xFF, {0xFE, 0x06, 0xF2, type, 1, 2, 3, 4}}}};
    const auto info = protocol.QueryFirmwareInfo(std::ref(exchange));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->node_id, 9);
    EXPECT_EQ(info->type, static_cast<EyouMotorFirmwareType>(type));
    EXPECT_EQ(info->protocol, EyouMotorVersionProtocol::Boot);
    EXPECT_EQ(info->version, "1.2.3.4");
    EXPECT_EQ(info->version_bytes, (std::array<uint8_t, 4>{1, 2, 3, 4}));
    ASSERT_EQ(exchange.requests.size(), 1);
    EXPECT_EQ(exchange.requests[0].can_id, 9);
    EXPECT_EQ(exchange.reply_ids[0], 0xFF);
  }
}

TEST(EyouMotorOtaProtocolTest, UnknownBootQueryTypeIsNotAssumedToBeBootloader) {
  const EyouMotorOtaProtocol protocol(1);
  ScriptedExchange exchange{{EyouMotorOtaFrame{
      0xFF, {0xFE, 0x06, 0xF2, 0x33, 1, 2, 3, 4}}}};
  const auto info = protocol.QueryFirmwareInfo(std::ref(exchange));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->type, EyouMotorFirmwareType::Unknown);
  EXPECT_EQ(exchange.requests.size(), 1);
}

TEST(EyouMotorOtaProtocolTest, MissingBootReplyFallsBackToCanopenExample) {
  const EyouMotorOtaProtocol protocol(2);
  ScriptedExchange exchange{{
      std::nullopt,
      EyouMotorOtaFrame{0x582, {0x41, 0x0A, 0x10, 0, 0x0B, 0, 0, 0}},
      EyouMotorOtaFrame{0x582, {0x00, 0x56, 0x31, 0x34, 0x37, 0x2D, 0x30, 0x32}},
      EyouMotorOtaFrame{0x582, {0x17, 0x36, 0x34, 0, 0, 0, 0, 0}}}};
  const auto info = protocol.QueryFirmwareInfo(std::ref(exchange));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->node_id, 2);
  EXPECT_EQ(info->type, EyouMotorFirmwareType::App);
  EXPECT_EQ(info->protocol, EyouMotorVersionProtocol::Canopen);
  EXPECT_EQ(info->version, "V147-0264");
  ASSERT_EQ(exchange.requests.size(), 4);
  EXPECT_EQ(exchange.requests[1].data,
            (Payload{0x40, 0x0A, 0x10, 0, 0, 0, 0, 0}));
  EXPECT_EQ(exchange.requests[2].data[0], 0x60);
  EXPECT_EQ(exchange.requests[3].data[0], 0x70);
}

TEST(EyouMotorOtaProtocolTest, CanopenExpeditedVersionIsApp) {
  const EyouMotorOtaProtocol protocol(21);
  ScriptedExchange exchange{{std::nullopt, EyouMotorOtaFrame{
      0x595, {0x47, 0x0A, 0x10, 0, '1', '.', '2', 0xFF}}}};
  const auto info = protocol.QueryFirmwareInfo(std::ref(exchange));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->type, EyouMotorFirmwareType::App);
  EXPECT_EQ(info->version, "1.2");
  EXPECT_EQ(exchange.reply_ids[1], 0x595);
}

TEST(EyouMotorOtaProtocolTest, CanopenLongVersionTogglesEverySegment) {
  const EyouMotorOtaProtocol protocol(17);
  ScriptedExchange exchange{{
      std::nullopt,
      EyouMotorOtaFrame{0x591, {0x41, 0x0A, 0x10, 0, 16, 0, 0, 0}},
      EyouMotorOtaFrame{0x591, {0x00, 'A', 'B', 'C', 'D', 'E', 'F', 'G'}},
      EyouMotorOtaFrame{0x591, {0x10, 'H', 'I', 'J', 'K', 'L', 'M', 'N'}},
      EyouMotorOtaFrame{0x591, {0x0B, 'O', 'P', 0, 0, 0, 0, 0}}}};
  const auto info = protocol.QueryFirmwareInfo(std::ref(exchange));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->version, "ABCDEFGHIJKLMNOP");
  EXPECT_EQ(exchange.requests[4].data[0], 0x60);
}

TEST(EyouMotorOtaProtocolTest, MissingBothQueriesDoesNotInventFirmwareMode) {
  const EyouMotorOtaProtocol protocol(1);
  ScriptedExchange exchange{{std::nullopt, std::nullopt}};
  EXPECT_FALSE(protocol.QueryFirmwareInfo(std::ref(exchange)));
  EXPECT_EQ(exchange.requests.size(), 2);
}

TEST(EyouMotorOtaProtocolTest, BootParserRejectsOtherIdsAndMalformedFrames) {
  const EyouMotorOtaProtocol protocol(1);
  const EyouMotorOtaFrame valid{
      0xFF, {0xFE, 0x06, 0xF2, 0x55, 1, 2, 3, 4}};
  auto invalid = valid;
  invalid.can_id = 1;
  EXPECT_FALSE(protocol.ParseBootFirmwareInfo(invalid));
  invalid = valid;
  invalid.size = 7;
  EXPECT_FALSE(protocol.ParseBootFirmwareInfo(invalid));
  for (const size_t index : {0, 1, 2}) {
    invalid = valid;
    invalid.data[index] ^= 1;
    EXPECT_FALSE(protocol.ParseBootFirmwareInfo(invalid));
  }
}

TEST(EyouMotorOtaProtocolTest, InvalidBootFrameAllowsCanopenFallback) {
  const EyouMotorOtaProtocol protocol(1);
  ScriptedExchange exchange{{
      EyouMotorOtaFrame{0xFF, {0xFE, 0x06, 0xB2, 0x55, 1, 2, 3, 4}},
      EyouMotorOtaFrame{0x581, {0x47, 0x0A, 0x10, 0, '1', '.', '2', 0}}}};
  const auto info = protocol.QueryFirmwareInfo(std::ref(exchange));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->protocol, EyouMotorVersionProtocol::Canopen);
}

TEST(EyouMotorOtaProtocolTest, CanopenAbortsWrongObjectsAndLengthsAreRejected) {
  const EyouMotorOtaProtocol protocol(1);
  for (const EyouMotorOtaFrame invalid : {
           EyouMotorOtaFrame{0x581, {0x80, 0x0A, 0x10, 0, 0, 0, 0, 0}},
           EyouMotorOtaFrame{0x582, {0x41, 0x0A, 0x10, 0, 11, 0, 0, 0}},
           EyouMotorOtaFrame{0x581, {0x41, 0x09, 0x10, 0, 11, 0, 0, 0}},
           EyouMotorOtaFrame{0x581, {0x41, 0x0A, 0x10, 0, 0xFF, 0xFF, 0, 0}},
           EyouMotorOtaFrame{0x581, {0x41, 0x0A, 0x10, 0, 11, 0, 0, 0}, 7}}) {
    ScriptedExchange exchange{{std::nullopt, invalid}};
    EXPECT_FALSE(protocol.QueryFirmwareInfo(std::ref(exchange)));
  }
}

TEST(EyouMotorOtaProtocolTest, CanopenWrongToggleAndShortTransfersAreRejected) {
  const EyouMotorOtaProtocol protocol(1);
  for (const EyouMotorOtaFrame invalid : {
           EyouMotorOtaFrame{0x581, {0x10, 'A', 'B', 'C', 'D', 'E', 'F', 'G'}},
           EyouMotorOtaFrame{0x581, {0x80, 0, 0, 0, 0, 0, 0, 0}},
           EyouMotorOtaFrame{0x581, {0x01, 'A', 'B', 'C', 'D', 'E', 'F', 'G'}}}) {
    ScriptedExchange exchange{{std::nullopt,
        EyouMotorOtaFrame{0x581, {0x41, 0x0A, 0x10, 0, 11, 0, 0, 0}}, invalid}};
    EXPECT_FALSE(protocol.QueryFirmwareInfo(std::ref(exchange)));
  }
}

TEST(EyouMotorOtaProtocolTest, CommandsMatchReferenceWireBytes) {
  const EyouMotorOtaProtocol protocol(21);
  const auto boot = protocol.EnterBootFromApp();
  EXPECT_EQ(boot.can_id, 0x615);
  EXPECT_EQ(boot.data, (Payload{0x23, 0x09, 0x21, 0x00,
                               0x78, 0x56, 0x34, 0x12}));
  EXPECT_EQ(protocol.CheckBootNode().can_id, 21);
  EXPECT_EQ(protocol.CheckBootNode().data,
            (Payload{0xFE, 0x06, 0xB2, 0x80, 0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(protocol.Erase(0x01020304).data,
            (Payload{0xFE, 0x06, 0xB4, 0x42, 1, 2, 4, 0}));
  EXPECT_EQ(protocol.PrepareWrite(0x12345678).data,
            (Payload{0xFE, 0x06, 0xB4, 0x03, 0x12, 0x34, 0x56, 0x78}));
  EXPECT_EQ(protocol.EnterApp().data,
            (Payload{0xFE, 0x06, 0xB4, 0x06, 0x41, 0x50, 0x50, 0xFF}));
}

TEST(EyouMotorOtaProtocolTest, AsciiExampleAndXmodemCheckVector) {
  const EyouMotorOtaProtocol protocol(1);
  const std::array<uint8_t, 9> data{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  const auto frames = protocol.WriteBlock(data.data(), data.size());
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[0].data,
            (Payload{0xFE, 0x10, 0x0B, 0xB4, 0xC4, 0x31, 0x32, 0x33}));
  EXPECT_EQ(frames[1].data,
            (Payload{0xFE, 0x21, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39}));
  EXPECT_EQ(protocol.CheckBlockCrc(data.data(), data.size()).data,
            (Payload{0xFE, 0x06, 0xB4, 0x85, 0x31, 0xC3, 0xFF, 0xFF}));
}

TEST(EyouMotorOtaProtocolTest, FullBlockWrapsSequenceAndHasReferenceCrc) {
  const EyouMotorOtaProtocol protocol(1);
  std::array<uint8_t, 1024> data{};
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i);
  }
  const auto frames = protocol.WriteBlock(data.data(), data.size());
  ASSERT_EQ(frames.size(), 172);
  EXPECT_EQ(frames.front().data,
            (Payload{0xFE, 0x14, 0x02, 0xB4, 0xC4, 0, 1, 2}));
  EXPECT_EQ(frames[15].data[1], 0x2F);
  EXPECT_EQ(frames[16].data[1], 0x20);
  EXPECT_EQ(frames.back().data,
            (Payload{0xFE, 0x2B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(protocol.CheckBlockCrc(data.data(), data.size()).data,
            (Payload{0xFE, 0x06, 0xB4, 0x85, 0xC2, 0xE0, 0xFF, 0xFF}));
}

TEST(EyouMotorOtaProtocolTest, AllBlockLengthsRoundTripWithFfPadding) {
  const EyouMotorOtaProtocol protocol(21);
  std::array<uint8_t, 1024> data{};
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>((i * 37 + 11) & 0xFF);
  }
  for (size_t size = 1; size <= data.size(); ++size) {
    SCOPED_TRACE(size);
    const auto frames = protocol.WriteBlock(data.data(), size);
    std::vector<uint8_t> restored;
    if (size <= 4) {
      ASSERT_EQ(frames.size(), 1);
      const auto& first = frames.front().data;
      EXPECT_EQ(first[1], size + 2);
      EXPECT_EQ(first[2], 0xB4);
      EXPECT_EQ(first[3], 0xC4);
      restored.insert(restored.end(), first.begin() + 4, first.end());
    } else {
      const auto& first = frames.front().data;
      EXPECT_EQ(((first[1] & 0x0F) << 8) | first[2], size + 2);
      EXPECT_EQ(first[3], 0xB4);
      EXPECT_EQ(first[4], 0xC4);
      restored.insert(restored.end(), first.begin() + 5, first.end());
      for (size_t index = 1; index < frames.size(); ++index) {
        EXPECT_EQ(frames[index].data[1], 0x20 | (index % 16));
        restored.insert(restored.end(), frames[index].data.begin() + 2,
                        frames[index].data.end());
      }
    }
    ASSERT_GE(restored.size(), size);
    EXPECT_TRUE(std::equal(data.begin(), data.begin() + size, restored.begin()));
    EXPECT_TRUE(std::all_of(restored.begin() + size, restored.end(),
                            [](uint8_t value) { return value == 0xFF; }));
    for (const auto& frame : frames) {
      EXPECT_EQ(frame.can_id, 21);
      EXPECT_EQ(frame.data[0], 0xFE);
    }
  }
}

TEST(EyouMotorOtaProtocolTest, EraseAlignsUpToOneKiBWithoutOverflow) {
  const EyouMotorOtaProtocol protocol(1);
  for (const uint32_t size : {1u, 1023u, 1024u}) {
    EXPECT_EQ(protocol.Erase(size).data,
              (Payload{0xFE, 0x06, 0xB4, 0x42, 0, 0, 4, 0}));
  }
  EXPECT_EQ(protocol.Erase(1025).data,
            (Payload{0xFE, 0x06, 0xB4, 0x42, 0, 0, 8, 0}));
  EXPECT_EQ(protocol.Erase(0xFFFFFC00u).data,
            (Payload{0xFE, 0x06, 0xB4, 0x42, 0xFF, 0xFF, 0xFC, 0}));
  EXPECT_THROW(protocol.Erase(0xFFFFFC01u), std::out_of_range);
  EXPECT_THROW(protocol.Erase(0xFFFFFFFFu), std::out_of_range);
  EXPECT_EQ(EyouMotorOtaProtocol::kEraseTimeoutMs, 5000);
}

TEST(EyouMotorOtaProtocolTest, BlockAddressesAreFileOffsetsAndTailLengthIsExact) {
  const EyouMotorOtaProtocol protocol(1);
  EXPECT_EQ(protocol.PrepareWrite(0).data,
            (Payload{0xFE, 0x06, 0xB4, 0x03, 0, 0, 0, 0}));
  EXPECT_EQ(protocol.PrepareWrite(1024).data,
            (Payload{0xFE, 0x06, 0xB4, 0x03, 0, 0, 4, 0}));
  EXPECT_EQ(protocol.PrepareWrite(2048).data,
            (Payload{0xFE, 0x06, 0xB4, 0x03, 0, 0, 8, 0}));
  const std::array<uint8_t, 4> data{1, 2, 3, 4};
  const auto frames = protocol.WriteBlock(data.data(), data.size());
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0].data,
            (Payload{0xFE, 0x06, 0xB4, 0xC4, 1, 2, 3, 4}));
}

TEST(EyouMotorOtaProtocolTest, ShortBlocksMatchReferenceSingleFrames) {
  const EyouMotorOtaProtocol protocol(4);
  const std::array<uint8_t, 4> data{0, 1, 2, 3};
  const std::array<Payload, 4> expected{{
      {0xFE, 0x03, 0xB4, 0xC4, 0, 0xFF, 0xFF, 0xFF},
      {0xFE, 0x04, 0xB4, 0xC4, 0, 1, 0xFF, 0xFF},
      {0xFE, 0x05, 0xB4, 0xC4, 0, 1, 2, 0xFF},
      {0xFE, 0x06, 0xB4, 0xC4, 0, 1, 2, 3},
  }};
  for (size_t size = 1; size <= data.size(); ++size) {
    SCOPED_TRACE(size);
    const auto frames = protocol.WriteBlock(data.data(), size);
    ASSERT_EQ(frames.size(), 1);
    EXPECT_EQ(frames[0].can_id, 4);
    EXPECT_EQ(frames[0].size, 8);
    EXPECT_EQ(frames[0].data, expected[size - 1]);
  }
}

TEST(EyouMotorOtaChannelTest, ReceivesDuringSendAndFiltersUnrelatedFrames) {
  EyouMotorOtaChannel* channel_ptr = nullptr;
  EyouMotorOtaChannel channel([&](const EyouMotorOtaFrame&) {
    channel_ptr->OnFrame({0x581, {}});
    channel_ptr->OnFrame({0xFF, {}, 7});
    channel_ptr->OnFrame({0xFF, {0xFE, 0x06, 0xF2, 0x55, 1, 2, 3, 4}});
    return true;
  });
  channel_ptr = &channel;
  const auto reply = channel.Exchange(EyouMotorOtaProtocol(1).CheckBootNode(),
                                       0xFF, 100);
  ASSERT_TRUE(reply);
  EXPECT_EQ(reply->data[3], 0x55);
}

TEST(EyouMotorOtaChannelTest, SendFailureExceptionAndTimeoutClearPendingReply) {
  int mode = 0;
  EyouMotorOtaChannel channel([&](const EyouMotorOtaFrame&) {
    if (mode == 1) {
      throw std::runtime_error("send failed");
    }
    return mode == 2;
  });
  const auto request = EyouMotorOtaProtocol(1).CheckBootNode();
  EXPECT_FALSE(channel.Exchange(request, 0xFF, 1));
  mode = 1;
  EXPECT_THROW(channel.Exchange(request, 0xFF, 1), std::runtime_error);
  channel.OnFrame({0xFF, {0xFE, 0x06, 0xF2, 0x55, 1, 2, 3, 4}});
  mode = 2;
  EXPECT_FALSE(channel.Exchange(request, 0xFF, 1));
}

TEST(EyouMotorOtaProtocolTest, InvalidNodeAndBlockInputsAreRejected) {
  EXPECT_THROW((EyouMotorOtaProtocol(0)), std::invalid_argument);
  EXPECT_THROW((EyouMotorOtaProtocol(22)), std::invalid_argument);
  EXPECT_THROW((EyouMotorOtaProtocol(128)), std::invalid_argument);
  const EyouMotorOtaProtocol protocol(1);
  const std::array<uint8_t, 1025> data{};
  EXPECT_THROW(protocol.Erase(0), std::invalid_argument);
  EXPECT_THROW(protocol.WriteBlock(nullptr, 1), std::invalid_argument);
  EXPECT_THROW(protocol.WriteBlock(data.data(), 0), std::invalid_argument);
  EXPECT_THROW(protocol.WriteBlock(data.data(), data.size()), std::invalid_argument);
  EXPECT_THROW(protocol.CheckBlockCrc(nullptr, 1), std::invalid_argument);
  EXPECT_THROW(protocol.CheckBlockCrc(data.data(), 0), std::invalid_argument);
  EXPECT_THROW(protocol.CheckBlockCrc(data.data(), data.size()), std::invalid_argument);
}

}  // namespace
}  // namespace omnihand
}  // namespace agilink
