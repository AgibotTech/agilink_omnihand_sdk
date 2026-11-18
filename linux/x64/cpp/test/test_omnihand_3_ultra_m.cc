// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <gtest/gtest.h>
#include "timestamp_test_utils.h"
#include "agilink_logger.h"
#include "omnihand/omnihand_3_ultra_m.h"
#ifdef OMNIHAND_BUILD_INTERNAL_TESTS
#include "omnihand/i_omnihand_calibrator.h"
#endif
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using agilink::AgilinkLogger;

static constexpr const char* TAG = "OmniHand3UltraMTest";
constexpr agilink::omnihand::h3um::H3UMGesture kTestGestures[] = {
    agilink::omnihand::h3um::H3UMGesture::H3U_M_GESTURE_SIDE_FIST,
    agilink::omnihand::h3um::H3UMGesture::H3U_M_GESTURE_OPEN};
constexpr uint8_t kTestJointIndex = 4;  // Pinky DIP changes in both presets.

static int g_request_interval = 5;
static std::string g_device_type = "zlgcan";
static std::string g_can_interface = "can0";
static uint8_t g_can_device_index = 0;
static uint8_t g_can_channel = 0;
static uint8_t g_device_id = 0;  // Discover the address and detect the revision.
static agilink::omnihand::HandType g_hand_type =
    agilink::omnihand::HandType::LEFT;

static int GetRequestInterval() {
  return g_request_interval;
}

static std::string GetDeviceType() {
  return g_device_type;
}

template <typename T>
static std::string ValuesToString(const std::vector<T>& values) {
  std::ostringstream stream;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) stream << ", ";
    stream << static_cast<long long>(values[i]);
  }
  return stream.str();
}

class OmniHand3UltraMTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string device_type = GetDeviceType();
    if (device_type == "hcan") {
      hand_ = agilink::omnihand::OmniHand3UltraM::createHandByHcan(
          g_hand_type, g_device_id,
          g_can_device_index, g_can_channel);
      AgilinkLogger::get().infof(TAG, "Using HCAN device");
#ifdef __linux__
    } else if (device_type == "socketcan") {
      hand_ = agilink::omnihand::OmniHand3UltraM::createHandSocketCan(
          g_hand_type, g_device_id, g_can_interface);
      AgilinkLogger::get().infof(TAG, "Using SocketCAN device (%s)", g_can_interface.c_str());
#endif
    } else {
      hand_ = agilink::omnihand::OmniHand3UltraM::createHandByZlgcan(
          g_hand_type, g_device_id,
          g_can_device_index, g_can_channel);
      AgilinkLogger::get().infof(TAG, "Using ZLG CAN device");
    }
    if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
    AgilinkLogger::get().infof(
        TAG, "Hand: %s, active DOF: %u",
        g_hand_type == agilink::omnihand::HandType::LEFT ? "left" : "right",
        static_cast<unsigned>(hand_->GetDoA()));
    int request_interval = GetRequestInterval();
    hand_->SetRequestInterval(request_interval);
    if (hand_->GetRevision() == agilink::omnihand::h3um::H3UMRevision::T1P5) {
      mix_ctrl_current_ma_ =
          agilink::omnihand::OmniHand3UltraM::kT1P5MixCtrlTorqueRange.default_value;
    }
    if (request_interval != 0) {
      AgilinkLogger::get().infof(TAG, "Using request interval: %d ms", request_interval);
    }
  }

  void TearDown() override {
    hand_.reset();
  }

  std::unique_ptr<agilink::omnihand::OmniHand3UltraM> hand_;
  int16_t mix_ctrl_current_ma_ =
      agilink::omnihand::OmniHand3UltraM::kT1P3MixCtrlTorqueRange.default_value;
};

// Control tests use revision-specific side-fist and open presets.
// Timestamp tests preserve raw feedback instead of enforcing command bounds.
TEST_F(OmniHand3UltraMTest, GetAllJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
  AgilinkLogger::get().infof(TAG, "Motor positions: %s",
                            ValuesToString(sample.value).c_str());
}

TEST_F(OmniHand3UltraMTest, GetAllErrorReportWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllErrorReportWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllErrorReportWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
}

TEST_F(OmniHand3UltraMTest, GetAllTemperatureReportWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllTemperatureReportWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllTemperatureReportWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
}

TEST_F(OmniHand3UltraMTest, GetTemperatureOfPalmWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  if (hand_->GetRevision() != agilink::omnihand::h3um::H3UMRevision::T1P5) {
    GTEST_SKIP() << "Board temperature requires T1.5";
  }
  const auto sample = hand_->GetTemperatureOfPalmWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
      sample.timestamp_us, "GetTemperatureOfPalmWithTimestamp", TAG));
  AgilinkLogger::get().infof(TAG, "[GetTemperatureOfPalmWithTimestamp] temperature=%d", sample.value);
}

TEST_F(OmniHand3UltraMTest, GetTemperatureOfPowerWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  if (hand_->GetRevision() != agilink::omnihand::h3um::H3UMRevision::T1P5) {
    GTEST_SKIP() << "Board temperature requires T1.5";
  }
  const auto sample = hand_->GetTemperatureOfPowerWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
      sample.timestamp_us, "GetTemperatureOfPowerWithTimestamp", TAG));
  AgilinkLogger::get().infof(TAG, "[GetTemperatureOfPowerWithTimestamp] temperature=%d", sample.value);
}

TEST_F(OmniHand3UltraMTest, GetFingerTactileWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  for (uint8_t index = 0; index < 5; ++index) {
    agilink::omnihand::XenseFrame frame;
    const auto sample = hand_->GetFingerTactileWithTimestamp(index, frame);
    if (!sample.value) {
      GTEST_SKIP() << "Xense tactile sensor unavailable";
    }
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "GetFingerTactileWithTimestamp", TAG));
    const uint64_t source_timestamp = frame.timestamp;
    const auto cached = hand_->GetFingerTactileWithTimestamp(index, frame);
    if (cached.value && source_timestamp != 0 && source_timestamp == frame.timestamp) {
      EXPECT_EQ(cached.timestamp_us, sample.timestamp_us);
    }
  }
}

TEST_F(OmniHand3UltraMTest, GetPalmTactileWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  agilink::omnihand::PalmFrame frame;
  const auto sample = hand_->GetPalmTactileWithTimestamp(frame);
  if (!sample.value || frame.force.empty()) {
    GTEST_SKIP() << "Palm tactile sensor unavailable";
  }
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
      sample.timestamp_us, "GetPalmTactileWithTimestamp", TAG));
  const double source_timestamp = frame.timestamp;
  const auto cached = hand_->GetPalmTactileWithTimestamp(frame);
  if (cached.value && source_timestamp != 0 && source_timestamp == frame.timestamp) {
    EXPECT_EQ(cached.timestamp_us, sample.timestamp_us);
  }
}

TEST_F(OmniHand3UltraMTest, GetAllTactileWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  agilink::omnihand::AllTactileFrame frame;
  const auto sample = hand_->GetAllTactileWithTimestamp(frame);
  if (!sample.value) {
    GTEST_SKIP() << "Tactile sensors unavailable";
  }
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
      sample.timestamp_us, "GetAllTactileWithTimestamp", TAG));
}

TEST_F(OmniHand3UltraMTest, GetAllCurrentReportWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllCurrentReportWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllCurrentReportWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
}

TEST_F(OmniHand3UltraMTest, GetJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetJointMotorPosiWithTimestamp(1);
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetJointMotorPosiWithTimestamp", TAG));
  AgilinkLogger::get().infof(TAG, "Joint 1 position: %d", sample.value);
}

TEST_F(OmniHand3UltraMTest, SetJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  for (const auto gesture : kTestGestures) {
    SCOPED_TRACE(static_cast<int>(gesture));
    const auto targets = hand_->GetHandGesture(gesture);
    ASSERT_EQ(targets.size(), hand_->GetDoA());
    const auto sample = hand_->SetJointMotorPosiWithTimestamp(
        kTestJointIndex, targets[kTestJointIndex - 1]);
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "SetJointMotorPosiWithTimestamp", TAG));
    AgilinkLogger::get().infof(TAG, "Joint %u target=%d, feedback=%d",
                              static_cast<unsigned>(kTestJointIndex),
                              targets[kTestJointIndex - 1], sample.value);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

TEST_F(OmniHand3UltraMTest, SetAllJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  for (const auto gesture : kTestGestures) {
    SCOPED_TRACE(static_cast<int>(gesture));
    const auto targets = hand_->GetHandGesture(gesture);
    ASSERT_EQ(targets.size(), hand_->GetDoA());
    const auto sample = hand_->SetAllJointMotorPosiWithTimestamp(targets);
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "SetAllJointMotorPosiWithTimestamp", TAG));
    ASSERT_EQ(sample.value.size(), targets.size());
    AgilinkLogger::get().infof(TAG, "Gesture %d feedback: %s",
                              static_cast<int>(gesture),
                              ValuesToString(sample.value).c_str());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

TEST_F(OmniHand3UltraMTest, GetAllActiveJointAnglesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllActiveJointAnglesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllActiveJointAnglesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
  for (const auto angle : sample.value) EXPECT_TRUE(std::isfinite(angle));
}

TEST_F(OmniHand3UltraMTest, GetAllJointAnglesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllJointAnglesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllJointAnglesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetJointNames().size());
  for (const auto angle : sample.value) EXPECT_TRUE(std::isfinite(angle));
}

TEST_F(OmniHand3UltraMTest, SetAllActiveJointAnglesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const agilink::omnihand::h3um::OmniHand3UltraMSolver solver(
      hand_->GetHandType() == agilink::omnihand::HandType::LEFT,
      hand_->GetRevision());
  for (const auto gesture : kTestGestures) {
    SCOPED_TRACE(static_cast<int>(gesture));
    const auto targets = hand_->GetHandGesture(gesture);
    ASSERT_EQ(targets.size(), hand_->GetDoA());
    const auto angles = solver.ActuatorInput2ActiveJointPos(targets);
    ASSERT_EQ(angles.size(), targets.size());
    const auto sample = hand_->SetAllActiveJointAnglesWithTimestamp(angles);
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "SetAllActiveJointAnglesWithTimestamp", TAG));
    ASSERT_EQ(sample.value.size(), angles.size());
    for (const auto angle : sample.value) EXPECT_TRUE(std::isfinite(angle));
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

// H3UM mixed-control tests cover the supported PT mode only.
TEST_F(OmniHand3UltraMTest, MixControlByPTAllWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  for (const auto gesture : kTestGestures) {
    SCOPED_TRACE(static_cast<int>(gesture));
    const auto targets = hand_->GetHandGesture(gesture);
    ASSERT_EQ(targets.size(), hand_->GetDoA());
    const auto sample = hand_->MixControlByPTWithTimestamp(
        targets, std::vector<int16_t>(targets.size(), mix_ctrl_current_ma_));
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "MixControlByPTAllWithTimestamp", TAG));
    ASSERT_EQ(sample.value.size(), targets.size());
    for (const auto& feedback : sample.value) {
      EXPECT_TRUE(feedback.tgt_posi_.has_value());
      EXPECT_TRUE(feedback.tgt_torque_.has_value());
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

TEST_F(OmniHand3UltraMTest, MixControlByPTSingleWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  for (const auto gesture : kTestGestures) {
    SCOPED_TRACE(static_cast<int>(gesture));
    const auto targets = hand_->GetHandGesture(gesture);
    ASSERT_EQ(targets.size(), hand_->GetDoA());
    const auto sample = hand_->MixControlByPTWithTimestamp(
        kTestJointIndex, targets[kTestJointIndex - 1], mix_ctrl_current_ma_);
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "MixControlByPTSingleWithTimestamp", TAG));
    EXPECT_EQ(sample.value.joint_index_, kTestJointIndex);
    EXPECT_TRUE(sample.value.tgt_posi_.has_value());
    EXPECT_TRUE(sample.value.tgt_torque_.has_value());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

TEST_F(OmniHand3UltraMTest, GetAllTactileSensorDataRawWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  if (!hand_->InitTactilePointsMap()) GTEST_SKIP() << "Tactile sensors unavailable";
  if (auto* scalar_tactile = hand_->GetTactileSensor1D()) {
    const auto sample = scalar_tactile->GetAllTactileSensorDataRawWithTimestamp();
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllTactileSensorDataRawWithTimestamp", TAG));
    ASSERT_EQ(sample.value.size(), hand_->GetSensorOrder().size());
    for (const auto& sensor : sample.value) {
      EXPECT_EQ(sensor.data_.size(), scalar_tactile->GetNumOfTactilePoints(sensor.sensor_id_));
    }
  } else {
    auto* tactile = hand_->GetTactileSensor3D();
    ASSERT_NE(tactile, nullptr);
    const auto sample = tactile->GetAllTactileSensorDataRawWithTimestamp();
    ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllTactileSensorDataRawWithTimestamp", TAG));
    ASSERT_EQ(sample.value.size(), hand_->GetSensorOrder().size());
    for (const auto& sensor : sample.value) {
      EXPECT_EQ(sensor.points.size(), tactile->GetNumOfTactilePoints(sensor.sensor_id));
    }
  }
}

TEST_F(OmniHand3UltraMTest, GetAllTactileSensorResultantForcesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  auto* tactile = hand_->GetTactileSensor3D();
  if (!tactile) GTEST_SKIP() << "3D resultant forces require T1.3";
  const auto sample = tactile->GetAllTactileSensorResultantForcesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllTactileSensorResultantForcesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetSensorOrder().size());
}

TEST_F(OmniHand3UltraMTest, CreateHand) {
  EXPECT_NE(hand_, nullptr);
}

TEST_F(OmniHand3UltraMTest, Init) {
  bool init_result = hand_->Init();
}

TEST_F(OmniHand3UltraMTest, GetVendorInfo) {
  if (hand_->Init()) {
    auto vendor_info = hand_->GetVendorInfo();
    AgilinkLogger::get().infof(TAG, "[GetVendorInfo] %s", vendor_info.ToString().c_str());
    if (vendor_info.dof == 0) {
      AgilinkLogger::get().warnf(TAG, "[GetVendorInfo] Failed: got empty vendor info (timeout)");
      return;
    }
    const uint8_t doa = hand_->GetDoA();
    EXPECT_EQ(doa, vendor_info.dof);
    EXPECT_EQ(hand_->GetNumOfJointMotors(), doa);
    const auto joint_names = hand_->GetJointNames();
    EXPECT_EQ(joint_names.size(), doa);
  }
}

TEST_F(OmniHand3UltraMTest, GetDeviceInfo) {
  if (hand_->Init()) {
    auto device_info = hand_->GetDeviceInfo();
    AgilinkLogger::get().infof(TAG, "[GetDeviceInfo] %s", device_info.ToString().c_str());
    if (device_info.hand_device_id != 0) {
      EXPECT_EQ(device_info.hand_device_id, hand_->GetHandDeviceId());
    }
  }
}

TEST_F(OmniHand3UltraMTest, GetNonPrivateHandDeviceIdByBroadcast) {
  if (!hand_->Init()) return;

  const uint8_t device_id = hand_->GetNonPrivateHandDeviceIdByBroadcast();
  ASSERT_GT(device_id, 0u) << "No H3U_M replied to the broadcast request";
  EXPECT_EQ(hand_->GetHandDeviceId(), device_id);
}

TEST_F(OmniHand3UltraMTest, MixControlRejectsInvalidArguments) {
  const size_t dof = hand_->GetDoA();
  const std::vector<int16_t> valid(dof, 0);
  const std::vector<int16_t> short_values(dof - 1, 0);

  EXPECT_TRUE(hand_->MixControlByPT(short_values, short_values).empty());
  EXPECT_TRUE(hand_->MixControlByPT(valid, short_values).empty());

  const auto invalid_pt = hand_->MixControlByPT(0, 0, 0);
  EXPECT_EQ(invalid_pt.ctrl_mode_, 0);
  const auto invalid_pt_high = hand_->MixControlByPT(static_cast<uint8_t>(dof + 1), 0, 0);
  EXPECT_EQ(invalid_pt_high.ctrl_mode_, 0);
}

// TEST_F(OmniHand3UltraMTest, SetDeviceId) {
//   auto current_device_info = hand_->GetDeviceInfo();
//   unsigned char current_id = current_device_info.hand_device_id;
//   if (current_id == 0) return;

//   unsigned char target_id = 2;
//   hand_->SetDeviceId(target_id);
//   AgilinkLogger::get().infof(TAG, "[SetDeviceId] Set Device ID: %d", static_cast<int>(target_id));
//   std::this_thread::sleep_for(std::chrono::milliseconds(100));
//   auto device_info = hand_->GetDeviceInfo();
//   EXPECT_EQ(device_info.hand_device_id, 2);

//   unsigned char original_id = 9;
//   hand_->SetDeviceId(original_id);
//   AgilinkLogger::get().infof(TAG, "[SetDeviceId] Reset Device ID: %d", static_cast<int>(original_id));
//   std::this_thread::sleep_for(std::chrono::milliseconds(100));
//   auto device_info1 = hand_->GetDeviceInfo();
//   EXPECT_EQ(device_info1.hand_device_id, 9);
// }

TEST_F(OmniHand3UltraMTest, GestureDance) {
  if (!hand_->Init()) return;

  std::vector<unsigned char> pp_modes(hand_->GetDoA(), static_cast<unsigned char>(agilink::omnihand::ControlMode::PROFILE_POSITION));
  std::vector<unsigned char> csp_modes(hand_->GetDoA(), static_cast<unsigned char>(agilink::omnihand::ControlMode::POSITION));

  AgilinkLogger::get().infof(TAG, "[GestureDance] Switching to PP mode (PROFILE_POSITION=7)");
  hand_->SetAllControlMode(pp_modes);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  for (const auto gesture : kTestGestures) {
    SCOPED_TRACE(static_cast<int>(gesture));
    const auto sample = hand_->SetHandGestureWithTimestamp(static_cast<int>(gesture));
    EXPECT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
        sample.timestamp_us, "SetHandGestureWithTimestamp", TAG));
    EXPECT_EQ(sample.value.size(), hand_->GetDoA());
    AgilinkLogger::get().infof(TAG, "[GestureDance] Gesture %d feedback: %s",
                              static_cast<int>(gesture),
                              ValuesToString(sample.value).c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }

  AgilinkLogger::get().infof(TAG, "[GestureDance] Switching back to CSP mode (POSITION=0)");
  hand_->SetAllControlMode(csp_modes);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  AgilinkLogger::get().infof(TAG, "[GestureDance] Done!");
}

TEST_F(OmniHand3UltraMTest, ControlMode) {
  if (hand_->Init()) {
    auto current_modes = hand_->GetAllControlMode();
    if (current_modes.empty() || current_modes.size() != hand_->GetDoA()) {
      AgilinkLogger::get().warnf(TAG, "[GetAllControlMode] Failed: got %zu modes, expected %u",
                                current_modes.size(), static_cast<unsigned>(hand_->GetDoA()));
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAllControlMode] Control Modes: %s",
                              ValuesToString(current_modes).c_str());
    EXPECT_EQ(current_modes.size(), hand_->GetDoA());
  }
}

TEST_F(OmniHand3UltraMTest, ErrorReport) {
  if (hand_->Init()) {
    auto error_reports = hand_->GetAllErrorReport();
    if (error_reports.empty()) return;
    std::ostringstream reports;
    for (size_t i = 0; i < error_reports.size(); ++i) {
      if (i > 0) reports << " ";
      reports << "J" << (i + 1) << ":["
              << agilink::omnihand::H3UMErrorReportToString(error_reports[i]) << "]";
    }
    AgilinkLogger::get().infof(TAG, "[GetAllErrorReport] Error Reports: %s",
                              reports.str().c_str());
    EXPECT_EQ(error_reports.size(), hand_->GetDoA());
  }
}

TEST_F(OmniHand3UltraMTest, ClearErrorReport) {
  if (hand_->Init()) {
    hand_->ClearAllErrorReport();
    AgilinkLogger::get().infof(TAG, "[ClearAllErrorReport] Done");

    hand_->ClearErrorReport(1);
    AgilinkLogger::get().infof(TAG, "[ClearErrorReport] Joint 1 Done");
  }
}

TEST_F(OmniHand3UltraMTest, TemperatureReport) {
  if (hand_->Init()) {
    auto temp_reports = hand_->GetAllTemperatureReport();
    if (temp_reports.empty()) return;
    std::ostringstream reports;
    for (size_t i = 0; i < temp_reports.size(); ++i) {
      if (i > 0) reports << ", ";
      reports << "J" << (i + 1) << ":" << temp_reports[i];
    }
    AgilinkLogger::get().infof(TAG, "[GetAllTemperatureReport] Temperature Reports (C): %s",
                              reports.str().c_str());
    EXPECT_EQ(temp_reports.size(), hand_->GetDoA());
  }
}

TEST_F(OmniHand3UltraMTest, CurrentReport) {
  if (hand_->Init()) {
    auto current_reports = hand_->GetAllCurrentReport();
    if (current_reports.empty()) return;
    std::ostringstream reports;
    for (size_t i = 0; i < current_reports.size(); ++i) {
      if (i > 0) reports << ", ";
      reports << "J" << (i + 1) << ":" << current_reports[i];
    }
    AgilinkLogger::get().infof(TAG, "[GetAllCurrentReport] Current Reports (mA): %s",
                              reports.str().c_str());
    EXPECT_EQ(current_reports.size(), hand_->GetDoA());
  }
}

#ifdef OMNIHAND_BUILD_INTERNAL_TESTS
TEST_F(OmniHand3UltraMTest, AxisLimitPos) {
  if (hand_->Init()) {
    auto* maintenance =
        dynamic_cast<agilink::omnihand::IOmniHandCalibrator*>(hand_.get());
    ASSERT_NE(maintenance, nullptr);
    auto limits = maintenance->GetAxisLimitPos();
    if (limits.empty()) {
      AgilinkLogger::get().warnf(TAG, "[GetAxisLimitPos] Failed: empty result");
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAxisLimitPos] Min Limits (0.1 deg): %s",
                              ValuesToString(limits.min_limits).c_str());
    AgilinkLogger::get().infof(TAG, "[GetAxisLimitPos] Max Limits (0.1 deg): %s",
                              ValuesToString(limits.max_limits).c_str());
    EXPECT_EQ(limits.min_limits.size(), hand_->GetDoA());
    EXPECT_EQ(limits.max_limits.size(), hand_->GetDoA());
  }
}

TEST_F(OmniHand3UltraMTest, ActualAxisPos) {
  if (hand_->Init()) {
    auto* maintenance =
        dynamic_cast<agilink::omnihand::IOmniHandCalibrator*>(hand_.get());
    ASSERT_NE(maintenance, nullptr);
    auto actual_pos = maintenance->GetAllActualAxisPos();
    if (actual_pos.empty()) {
      AgilinkLogger::get().warnf(TAG, "[GetAllActualAxisPos] Failed: empty result");
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAllActualAxisPos] Actual Positions (0.1 deg): %s",
                              ValuesToString(actual_pos).c_str());
    EXPECT_EQ(actual_pos.size(), hand_->GetDoA());

    auto single = maintenance->GetSingleActualAxisPos(1);
    AgilinkLogger::get().infof(TAG, "[GetSingleActualAxisPos] Joint 1: %d", static_cast<int>(single));
  }
}

#endif

int main(int argc, char** argv) {
  std::vector<char*> gtest_args;
  gtest_args.push_back(argv[0]);

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" && i + 1 < argc) {
      try {
        int interval = std::stoi(argv[i + 1]);
        if (interval >= 0 && interval <= 100) {
          g_request_interval = interval;
          ++i;
          continue;
        } else {
          AgilinkLogger::get().errorf(TAG, "-f value %d is out of range (0-100ms)", interval);
          return 1;
        }
      } catch (const std::exception& e) {
        AgilinkLogger::get().errorf(TAG, "Invalid -f value: %s", argv[i + 1]);
        return 1;
      }
    } else if (arg == "-d" && i + 1 < argc) {
      std::string device_type = argv[i + 1];
      if (device_type == "zlgcan" || device_type == "hcan" || device_type == "socketcan") {
        g_device_type = device_type;
        ++i;
        continue;
      } else {
        AgilinkLogger::get().errorf(TAG,
                                   "-d value must be 'zlgcan', 'hcan' or 'socketcan', got: %s",
                                   device_type.c_str());
        return 1;
      }
    } else if (arg == "--hand" || arg == "--hand-type") {
      if (i + 1 == argc) {
        AgilinkLogger::get().errorf(TAG, "Missing value for %s", arg.c_str());
        return 1;
      }
      const std::string side = argv[++i];
      if (side == "left") {
        g_hand_type = agilink::omnihand::HandType::LEFT;
      } else if (side == "right") {
        g_hand_type = agilink::omnihand::HandType::RIGHT;
      } else {
        AgilinkLogger::get().errorf(TAG, "%s must be left or right", arg.c_str());
        return 1;
      }
      continue;
    } else if (arg == "--can-if") {
      if (i + 1 == argc || argv[i + 1][0] == '\0' || argv[i + 1][0] == '-') {
        AgilinkLogger::get().errorf(TAG, "Missing interface name for --can-if");
        return 1;
      }
      g_can_interface = argv[++i];
      continue;
    } else if (arg == "--revision") {
      AgilinkLogger::get().errorf(TAG, "Revision is detected automatically; --revision is not supported");
      return 1;
    } else if (arg == "--device-id" || arg == "-i" || arg == "-c") {
      if (i + 1 == argc) {
        AgilinkLogger::get().errorf(TAG, "Missing value for %s", arg.c_str());
        return 1;
      }
      try {
        size_t consumed = 0;
        const std::string value = argv[++i];
        const int number = std::stoi(value, &consumed);
        const int maximum = arg == "--device-id" ? 0x7f : 0xff;
        if (consumed != value.size() || number < 0 || number > maximum) {
          AgilinkLogger::get().errorf(TAG, "%s must be an integer in [0, %d]", arg.c_str(), maximum);
          return 1;
        }
        if (arg == "--device-id") {
          g_device_id = static_cast<uint8_t>(number);
        } else if (arg == "-i") {
          g_can_device_index = static_cast<uint8_t>(number);
        } else {
          g_can_channel = static_cast<uint8_t>(number);
        }
      } catch (const std::exception&) {
        AgilinkLogger::get().errorf(TAG, "Invalid value for %s", arg.c_str());
        return 1;
      }
      continue;
    } else if (arg == "--help" || arg == "-h") {
      AgilinkLogger::get().infof(TAG, "Usage: %s [-f INTERVAL] [-d DEVICE] [-i INDEX] [-c CHANNEL] [--can-if INTERFACE] [--device-id ID] [--hand left|right]", argv[0]);
      AgilinkLogger::get().infof(TAG, "  --device-id       Target hand ID (0-127, default: 0 for discovery); revision is detected automatically");
      AgilinkLogger::get().infof(TAG, "  -f INTERVAL       Set CAN request interval (0-100ms, default: 5ms)");
      AgilinkLogger::get().infof(TAG, "  -d DEVICE         Set CAN device type (zlgcan, hcan or socketcan, default: zlgcan)");
      AgilinkLogger::get().infof(TAG, "  -i INDEX          CAN adapter index (0-255, default: 0)");
      AgilinkLogger::get().infof(TAG, "  -c CHANNEL        CAN channel index (0-255, default: 0)");
      AgilinkLogger::get().infof(TAG, "  --can-if NAME     SocketCAN interface name (default: can0)");
      AgilinkLogger::get().infof(TAG, "  --hand SIDE       left or right (default: left; alias: --hand-type)");
      return 0;
    }
    gtest_args.push_back(argv[i]);
  }

  int gtest_argc = static_cast<int>(gtest_args.size());
  ::testing::InitGoogleTest(&gtest_argc, gtest_args.data());
  return RUN_ALL_TESTS();
}
