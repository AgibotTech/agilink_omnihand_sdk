// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <gtest/gtest.h>
#include <chrono>
#include <iterator>
#include <vector>
#include "omnihand/omnihand_3_lite.h"

using namespace agilink::omnihand;
using h3l::OmniHand3LiteGesture;
using h3l::OmniHand3LiteSolver;

namespace {
class RecordingH3L : public OmniHand3Lite {
 public:
  explicit RecordingH3L(HandType side = HandType::RIGHT) { Reset(1, side); }
  std::vector<int16_t> SetAllJointMotorPosi(const std::vector<int16_t>& positions) override {
    commands.push_back(positions);
    times.push_back(std::chrono::steady_clock::now());
    if (fail_next_write) {
      fail_next_write = false;
      return {};
    }
    return positions;
  }
  void ClearCommands() { commands.clear(); times.clear(); }
  std::vector<std::vector<int16_t>> commands;
  std::vector<std::chrono::steady_clock::time_point> times;
  bool fail_next_write = false;

  // Unused transport/private methods do not touch hardware.
  uint8_t GetDefaultNonPrivateHandDeviceId() const override { return 1; }
  uint16_t GetDefaultPrivateHandDeviceId() const override { return 1; }
  size_t GetNumOfTactileSensors() const override { return 0; }
  size_t GetNumOfTactilePoints(Finger) const override { return 0; }
  bool InitTactilePointsMap() override { return false; }
  size_t GetLenOfTactileDatum(Finger) const override { return 0; }
  size_t GetNumOfRepliedTactileFrames(Finger) const override { return 0; }
  std::string GetSNOfTactileSensor(Finger) const override { return {}; }
  size_t GetSensorDataLength(Finger) const override { return 0; }
  const std::vector<Finger>& GetSensorOrder() const override {
    static const std::vector<Finger> order;
    return order;
  }
  TactileSensorDataU16 GetTactileSensorData(Finger) const override { return {}; }
  TactileSensorDataU16 GetTactileSensorDataRaw(Finger) const override { return {}; }
  std::vector<TactileSensorDataU16> GetAllTactileSensorDataRaw() const override { return {}; }
  void ShowDataDetails(bool show, DataDetailsLogCallback log_callback ) const override { }
  void SetRequestInterval(int milliseconds) override { }
  int GetRequestInterval() const override { return {}; }
  void SetFrameRecvTimeout(int milliseconds) override { }
  int GetFrameRecvTimeout() const override { return {}; }
  void SetFrameSendTimeout(int milliseconds) override { }
  int GetFrameSendTimeout() const override { return {}; }
  bool SetPowerState(uint8_t state) override { return {}; }
  uint8_t GetPowerState() const override { return {}; }
  bool SetId(uint16_t id) override { return {}; }
  uint16_t SetSingleAxisPos(uint8_t axis_index, uint16_t position) override { return {}; }
  uint16_t GetSingleAxisPos(uint8_t axis_index) const override { return {}; }
  SetAllAxisPosResponse SetAllAxisPos(const std::vector<uint16_t>& positions) override { return {}; }
  std::vector<uint16_t> GetAllAxisPos() const override { return {}; }
  std::vector<int16_t> GetAllAxisCurrent() const override { return {}; }
  std::vector<int16_t> GetAllAxisVelocity() const override { return {}; }
  std::vector<int8_t> GetAllAxisTemp() const override { return {}; }
  uint16_t GetErrorCode() const override { return {}; }
  bool ClearError() override { return {}; }
  bool PlayAction(uint8_t action_id) override { return {}; }
  std::vector<uint16_t> GetAllAxisPosRange() const override { return {}; }
  std::vector<uint8_t> GetFingertipSensor(uint8_t sensor_index) const override { return {}; }
  std::vector<uint8_t> GetAllFingertipSensorA() const override { return {}; }
  std::vector<uint8_t> GetAllFingertipSensorB() const override { return {}; }
  std::vector<uint8_t> GetAllFingertipSensorC() const override { return {}; }
  bool SetRunMode(uint8_t motor_id, uint8_t mode) override { return {}; }
  std::vector<uint16_t> GetAllLoadData() const override { return {}; }
  bool SetAllRunSpeed(const std::vector<int16_t>& speeds) override { return {}; }
  bool SetOverloadTorque(uint8_t motor_id, uint16_t torque) override { return {}; }
  bool SetOverloadProtectionTime(uint8_t motor_id, uint16_t time) override { return {}; }
  bool SetProtectedTorque(uint8_t motor_id, uint8_t torque) override { return {}; }
  bool SetMinTorque(uint8_t axis_index, uint8_t min_force) override { return {}; }
  bool SetProtectiveCurrent(uint8_t axis_index, uint16_t current) override { return {}; }
  std::vector<int8_t> GetAllElectricMotorId() const override { return {}; }
  std::vector<int8_t> GetAllSensorId() const override { return {}; }
  bool SetAllAxisCvpUploadInterval(uint16_t interval_ms) override { return {}; }
  std::vector<uint8_t> GetAllAxisCvp() const override { return {}; }
  bool SetRightOrLeft(uint8_t hand_type) override { return {}; }
  SetAllAxisPosResponse SetPosSpeedTorqueData(const std::vector<uint16_t>& positions, const std::vector<int16_t>& speeds, const std::vector<uint16_t>& torques) override { return {}; }
  std::vector<uint8_t> GetFingerTactileForce() const override { return {}; }
  bool SetTemperatureThreshold(uint8_t threshold) override { return {}; }
  uint8_t SetControlSource(uint8_t source) override { return {}; }
  uint8_t GetControlSource() const override { return {}; }
  bool SetProductSerialNumber(const std::vector<uint8_t>& serial_number) override { return {}; }
  ProductSerialNumber GetProductSerialNumber() const override { return {}; }
  FirmwareVersionInfo GetFwVersion() const override { return {}; }
  int16_t SetSingleActualAxisPos(uint8_t axis_index, int16_t position) override { return {}; }
  int16_t GetSingleActualAxisPos(uint8_t axis_index) const override { return {}; }
  std::vector<int16_t> SetAllActualAxisPos(const std::vector<int16_t>& positions) override { return {}; }
  std::vector<int16_t> GetAllActualAxisPos() const override { return {}; }
  bool SetAxisHoming(uint8_t axis_index, int16_t pos) override { return {}; }
  bool SetAxisMinPos(uint8_t axis_index, int16_t min_pos) override { return {}; }
  bool SetAxisMaxPos(uint8_t axis_index, int16_t max_pos) override { return {}; }
  AxisLimitPos GetAxisLimitPos() const override { return {}; }
  bool ClearAllLimitPos() override { return {}; }
  bool SaveParam() override { return {}; }
};
}  // namespace

TEST(OmniHand3LiteGestureTest, EveryNonOpenPredecessorUsesThumbTransition) {
  const int count = static_cast<int>(std::size(OmniHand3LiteSolver::kH3LSGestureAngles));
  for (auto side : {HandType::RIGHT, HandType::LEFT}) {
    for (int index = 0; index < count; ++index) {
      SCOPED_TRACE(::testing::Message() << "side=" << static_cast<int>(side)
                                       << " predecessor=" << index);
      RecordingH3L hand(side);
      const auto previous = static_cast<OmniHand3LiteGesture>(index);
      const auto target = static_cast<OmniHand3LiteGesture>((index + 1) % count);
      hand.SetHandGesture(index);
      hand.ClearCommands();
      hand.SetHandGesture(target);
      const auto expected_target = hand.GetHandGesture(target);
      if (previous == OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OPEN) {
        ASSERT_EQ(hand.commands.size(), 1u);
        EXPECT_EQ(hand.commands[0], expected_target);
      } else {
        ASSERT_EQ(hand.commands.size(), 2u);
        auto transition = hand.GetHandGesture(previous);
        const auto open = hand.GetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OPEN);
        transition[2] = open[2];
        transition[3] = open[3];
        EXPECT_EQ(hand.commands[0], transition);
        EXPECT_EQ(hand.commands[1], expected_target);
        const auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            hand.times[1] - hand.times[0]);
        EXPECT_GE(delay.count(), 200);
      }
    }
  }
}

TEST(OmniHand3LiteGestureTest, FirstAndRepeatedPresetAreDirect) {
  RecordingH3L hand;
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK);
  ASSERT_EQ(hand.commands.size(), 1u);
  hand.ClearCommands();
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK);
  EXPECT_EQ(hand.commands.size(), 1u);
}

TEST(OmniHand3LiteGestureTest, QueryAndInvalidPresetDoNotChangePredecessor) {
  RecordingH3L hand;
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_LIKE);
  hand.ClearCommands();
  hand.GetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OPEN);
  hand.SetHandGesture(-1);
  hand.SetHandGesture(static_cast<int>(std::size(OmniHand3LiteSolver::kH3LSGestureAngles)));
  EXPECT_TRUE(hand.commands.empty());
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK);
  ASSERT_EQ(hand.commands.size(), 2u);
  const auto previous = hand.GetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_LIKE);
  EXPECT_EQ(hand.commands[0][0], previous[0]);
  EXPECT_EQ(hand.commands[0][1], previous[1]);
}

TEST(OmniHand3LiteGestureTest, PredecessorIsPerDeviceAndClearedOnSideChange) {
  RecordingH3L first;
  RecordingH3L second;
  first.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK);
  second.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_LIKE);
  EXPECT_EQ(second.commands.size(), 1u);
  first.ClearCommands();
  first.SetHandType(HandType::LEFT);
  first.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_LIKE);
  EXPECT_EQ(first.commands.size(), 1u);
}

TEST(OmniHand3LiteGestureTest, FailedTransitionDoesNotSendTarget) {
  RecordingH3L hand;
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_LIKE);
  hand.ClearCommands();
  hand.fail_next_write = true;
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK);
  EXPECT_EQ(hand.commands.size(), 1u);
}

TEST(OmniHand3LiteGestureTest, FailedFirstWriteIsNotCached) {
  RecordingH3L hand;
  hand.fail_next_write = true;
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_LIKE);
  hand.ClearCommands();
  hand.SetHandGesture(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK);
  EXPECT_EQ(hand.commands.size(), 1u);
}
