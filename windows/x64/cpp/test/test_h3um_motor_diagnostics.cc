// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <array>
#include <gtest/gtest.h>
#include "omnihand/impl/protocol_match.h"

using namespace agilink::omnihand;

namespace {
// Protocol bytes, independent of the host struct layout. Includes signed values,
// the high status bit, and counter boundaries.
constexpr std::array<uint8_t, 28> kReply{
    0xFF, 0x0F, 0x34, 0x02, 0x85, 0xFF, 0xC0, 0x5D,
    0x2E, 0xFB, 0xBF, 0xFE, 0x01, 0x00, 0x00, 0x80,
    0x38, 0xFF, 0xD2, 0x04, 0xFF, 0xFF, 0x00, 0x00,
    0x34, 0x12, 0xCD, 0xAB};
}

TEST(H3UMMotorDiagnostics, DecodesProtocolFieldsAndUnits) {
  H3UMMotorDiagnosticInfo info;
  ASSERT_TRUE(ParseH3UMMotorDiagnosticInfo(kReply.data(), kReply.size(), info));
  EXPECT_EQ(info.motor_inside_location, 4095);
  EXPECT_EQ(info.motor_outside_location, 564);
  EXPECT_EQ(info.motor_real_current, -123);
  EXPECT_EQ(info.motor_real_voltage, 24000);
  EXPECT_EQ(info.motor_real_speed, -1234);
  EXPECT_EQ(info.motor_real_temperature, -321);
  EXPECT_EQ(info.motor_real_status, 0x80000001u);
  EXPECT_EQ(info.motor_target_current, -200);
  EXPECT_EQ(info.motor_target_speed, 1234);
  EXPECT_EQ(info.pp_cmd_tx_count, 65535);
  EXPECT_EQ(info.pp_cmd_rx_count, 0);
  EXPECT_EQ(info.csp_cmd_tx_count, 0x1234);
  EXPECT_EQ(info.csp_cmd_rx_count, 0xABCD);
}

TEST(H3UMMotorDiagnostics, RejectsShortOrMissingReplyAndClearsOutput) {
  H3UMMotorDiagnosticInfo info;
  for (size_t length = 0; length < kReply.size(); ++length) {
    ASSERT_TRUE(ParseH3UMMotorDiagnosticInfo(kReply.data(), kReply.size(), info));
    EXPECT_FALSE(ParseH3UMMotorDiagnosticInfo(kReply.data(), length, info));
    const H3UMMotorDiagnosticInfo empty{};
    EXPECT_EQ(std::memcmp(&info, &empty, sizeof(info)), 0);
  }
  ASSERT_TRUE(ParseH3UMMotorDiagnosticInfo(kReply.data(), kReply.size(), info));
  EXPECT_FALSE(ParseH3UMMotorDiagnosticInfo(nullptr, kReply.size(), info));
  EXPECT_EQ(info.motor_real_status, 0u);
}

TEST(H3UMMotorDiagnostics, AcceptsCanFdPadding) {
  std::array<uint8_t, 32> padded;
  padded.fill(0xAA);
  std::copy(kReply.begin(), kReply.end(), padded.begin());
  H3UMMotorDiagnosticInfo info;
  ASSERT_TRUE(ParseH3UMMotorDiagnosticInfo(padded.data(), padded.size(), info));
  EXPECT_EQ(info.motor_real_status, 0x80000001u);
  EXPECT_EQ(info.csp_cmd_rx_count, 0xABCD);
}

TEST(H3UMMotorDiagnostics, MatchesEmptyQueryByRegisterMotorAndDevice) {
  UnCanId request;
  request.st_can_Id_.device_id_ = 1;
  request.st_can_Id_.product_id_ = 0x08;
  request.st_can_Id_.msg_type_ = static_cast<uint8_t>(MsgType::MOTOR_DIAGNOSTIC_INFO);
  request.st_can_Id_.msg_id_ = 1;
  EXPECT_EQ(request.ui_can_id_, 0x01810801u);
  const auto key = GetStandardReplyId(request.ui_can_id_);
  EXPECT_TRUE(JudgeStandardMsgMatch(key, request.ui_can_id_));
  request.st_can_Id_.msg_id_ = 2;
  EXPECT_FALSE(JudgeStandardMsgMatch(key, request.ui_can_id_));
  request.st_can_Id_.msg_id_ = 1;
  request.st_can_Id_.device_id_ = 2;
  EXPECT_FALSE(JudgeStandardMsgMatch(key, request.ui_can_id_));
}
