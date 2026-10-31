// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <gtest/gtest.h>
#include "timestamp_test_utils.h"
#include "agilink_logger.h"
#include "omnihand/omnihand_3_ultra_m.h"
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using agilink::AgilinkLogger;

static constexpr const char* TAG = "OmniHand3UltraMT1P3Test";

static int g_request_interval = 5;
static std::string g_device_type = "zlgcan";
static std::string g_can_interface = "can0";
static bool g_revision_t1p5 = false;
static uint8_t g_device_id = 9;  // Preserve the existing T1.3 test default.

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

class OmniHand3UltraMT1P3Test : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string device_type = GetDeviceType();
    if (device_type == "hcan") {
      hand_ = g_revision_t1p5 ?
          agilink::omnihand::OmniHand3UltraM::createHandT1P5ByHcan(
              agilink::omnihand::HandType::LEFT, g_device_id, 0, 0) :
          agilink::omnihand::OmniHand3UltraM::createHandByHcan(
              agilink::omnihand::HandType::LEFT, g_device_id, 0, 0);
      AgilinkLogger::get().infof(TAG, "Using HCAN device");
#ifdef __linux__
    } else if (device_type == "socketcan") {
      hand_ = g_revision_t1p5 ?
          agilink::omnihand::OmniHand3UltraM::createHandT1P5SocketCan(
              agilink::omnihand::HandType::LEFT, g_device_id, g_can_interface) :
          agilink::omnihand::OmniHand3UltraM::createHandSocketCan(
              agilink::omnihand::HandType::LEFT, g_device_id, g_can_interface);
      AgilinkLogger::get().infof(TAG, "Using SocketCAN device (%s)", g_can_interface.c_str());
#endif
    } else {
      hand_ = g_revision_t1p5 ?
          agilink::omnihand::OmniHand3UltraM::createHandT1P5ByZlgcan(
              agilink::omnihand::HandType::LEFT, g_device_id, 0, 0) :
          agilink::omnihand::OmniHand3UltraM::createHandByZlgcan(
              agilink::omnihand::HandType::LEFT, g_device_id, 0, 0);
      AgilinkLogger::get().infof(TAG, "Using ZLG CAN device");
    }
    int request_interval = GetRequestInterval();
    hand_->SetRequestInterval(request_interval);
    if (request_interval != 0) {
      AgilinkLogger::get().infof(TAG, "Using request interval: %d ms", request_interval);
    }
  }

  void TearDown() override {
    hand_.reset();
  }

  std::unique_ptr<agilink::omnihand::OmniHand3UltraM> hand_;
};

// Timestamped feedback tests. Controls hold the measured pose.
TEST_F(OmniHand3UltraMT1P3Test, GetAllJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
  for (const auto position : sample.value) { EXPECT_GE(position, 0); EXPECT_LE(position, 4095); }
}

TEST_F(OmniHand3UltraMT1P3Test, GetAllErrorReportWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllErrorReportWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllErrorReportWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
}

TEST_F(OmniHand3UltraMT1P3Test, GetAllTemperatureReportWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllTemperatureReportWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllTemperatureReportWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
}

TEST_F(OmniHand3UltraMT1P3Test, GetTemperatureOfPalmWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  if (hand_->GetRevision() != agilink::omnihand::H3UMRevision::T1P5) {
    GTEST_SKIP() << "Board temperature requires T1.5";
  }
  const auto sample = hand_->GetTemperatureOfPalmWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
      sample.timestamp_us, "GetTemperatureOfPalmWithTimestamp", TAG));
  AgilinkLogger::get().infof(TAG, "[GetTemperatureOfPalmWithTimestamp] temperature=%d", sample.value);
}

TEST_F(OmniHand3UltraMT1P3Test, GetTemperatureOfPowerWithTimestamp) {
  if (!hand_ || !hand_->Init()) {
    GTEST_SKIP() << "Device not available";
  }
  if (hand_->GetRevision() != agilink::omnihand::H3UMRevision::T1P5) {
    GTEST_SKIP() << "Board temperature requires T1.5";
  }
  const auto sample = hand_->GetTemperatureOfPowerWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(
      sample.timestamp_us, "GetTemperatureOfPowerWithTimestamp", TAG));
  AgilinkLogger::get().infof(TAG, "[GetTemperatureOfPowerWithTimestamp] temperature=%d", sample.value);
}

TEST_F(OmniHand3UltraMT1P3Test, GetFingerTactileWithTimestamp) {
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

TEST_F(OmniHand3UltraMT1P3Test, GetPalmTactileWithTimestamp) {
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

TEST_F(OmniHand3UltraMT1P3Test, GetAllTactileWithTimestamp) {
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

TEST_F(OmniHand3UltraMT1P3Test, GetAllCurrentReportWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllCurrentReportWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllCurrentReportWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
}

TEST_F(OmniHand3UltraMT1P3Test, GetJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetJointMotorPosiWithTimestamp(1);
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetJointMotorPosiWithTimestamp", TAG));
  EXPECT_GE(sample.value, 0); EXPECT_LE(sample.value, 4095);
}

TEST_F(OmniHand3UltraMT1P3Test, SetJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto position : measured.value) { ASSERT_GE(position, 0); ASSERT_LE(position, 4095); }
  const auto sample = hand_->SetJointMotorPosiWithTimestamp(1, measured.value.front());
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "SetJointMotorPosiWithTimestamp", TAG));
  EXPECT_GE(sample.value, 0); EXPECT_LE(sample.value, 4095);
}

TEST_F(OmniHand3UltraMT1P3Test, SetAllJointMotorPosiWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto position : measured.value) { ASSERT_GE(position, 0); ASSERT_LE(position, 4095); }
  const auto sample = hand_->SetAllJointMotorPosiWithTimestamp(measured.value);
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "SetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), measured.value.size());
}

TEST_F(OmniHand3UltraMT1P3Test, GetAllActiveJointAnglesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllActiveJointAnglesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllActiveJointAnglesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetDoA());
  for (const auto angle : sample.value) EXPECT_TRUE(std::isfinite(angle));
}

TEST_F(OmniHand3UltraMT1P3Test, GetAllJointAnglesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto sample = hand_->GetAllJointAnglesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllJointAnglesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetJointNames().size());
  for (const auto angle : sample.value) EXPECT_TRUE(std::isfinite(angle));
}

TEST_F(OmniHand3UltraMT1P3Test, SetAllActiveJointAnglesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllActiveJointAnglesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllActiveJointAnglesWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto angle : measured.value) ASSERT_TRUE(std::isfinite(angle));
  const auto sample = hand_->SetAllActiveJointAnglesWithTimestamp(measured.value);
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "SetAllActiveJointAnglesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), measured.value.size());
  for (const auto angle : sample.value) EXPECT_TRUE(std::isfinite(angle));
}

TEST_F(OmniHand3UltraMT1P3Test, MixControlByPTAllWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto position : measured.value) { ASSERT_GE(position, 0); ASSERT_LE(position, 4095); }
  const auto sample = hand_->MixControlByPTWithTimestamp(measured.value, std::vector<int16_t>(measured.value.size(), 0));
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "MixControlByPTAllWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), measured.value.size());
  for (const auto& feedback : sample.value) {
    EXPECT_TRUE(feedback.tgt_posi_.has_value());
    EXPECT_TRUE(feedback.tgt_torque_.has_value());
  }
}

TEST_F(OmniHand3UltraMT1P3Test, MixControlByPTSingleWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto position : measured.value) { ASSERT_GE(position, 0); ASSERT_LE(position, 4095); }
  const auto sample = hand_->MixControlByPTWithTimestamp(1, measured.value.front(), 0);
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "MixControlByPTSingleWithTimestamp", TAG));
  EXPECT_EQ(sample.value.joint_index_, 1);
  EXPECT_TRUE(sample.value.tgt_posi_.has_value());
  EXPECT_TRUE(sample.value.tgt_torque_.has_value());
}

TEST_F(OmniHand3UltraMT1P3Test, MixControlByPVTAllWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto position : measured.value) { ASSERT_GE(position, 0); ASSERT_LE(position, 4095); }
  const auto sample = hand_->MixControlByPVTWithTimestamp(measured.value, std::vector<int16_t>(measured.value.size(), 50), std::vector<int16_t>(measured.value.size(), 0));
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "MixControlByPVTAllWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), measured.value.size());
  for (const auto& feedback : sample.value) {
    EXPECT_TRUE(feedback.tgt_posi_.has_value());
    EXPECT_TRUE(feedback.tgt_torque_.has_value());
    EXPECT_TRUE(feedback.tgt_velo_.has_value());
  }
}

TEST_F(OmniHand3UltraMT1P3Test, MixControlByPVTSingleWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  const auto measured = hand_->GetAllJointMotorPosiWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(measured.timestamp_us, "GetAllJointMotorPosiWithTimestamp", TAG));
  ASSERT_EQ(measured.value.size(), hand_->GetDoA());
  for (const auto position : measured.value) { ASSERT_GE(position, 0); ASSERT_LE(position, 4095); }
  const auto sample = hand_->MixControlByPVTWithTimestamp(1, measured.value.front(), 50, 0);
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "MixControlByPVTSingleWithTimestamp", TAG));
  EXPECT_EQ(sample.value.joint_index_, 1);
  EXPECT_TRUE(sample.value.tgt_posi_.has_value());
  EXPECT_TRUE(sample.value.tgt_torque_.has_value());
  EXPECT_TRUE(sample.value.tgt_velo_.has_value());
}

TEST_F(OmniHand3UltraMT1P3Test, GetAllTactileSensorDataRawWithTimestamp) {
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

TEST_F(OmniHand3UltraMT1P3Test, GetAllTactileSensorResultantForcesWithTimestamp) {
  if (!hand_ || !hand_->Init()) GTEST_SKIP() << "Device not available";
  auto* tactile = hand_->GetTactileSensor3D();
  if (!tactile) GTEST_SKIP() << "3D resultant forces require T1.3";
  const auto sample = tactile->GetAllTactileSensorResultantForcesWithTimestamp();
  ASSERT_TRUE(timestamp_test::ValidateAndPrintTimestamp(sample.timestamp_us, "GetAllTactileSensorResultantForcesWithTimestamp", TAG));
  ASSERT_EQ(sample.value.size(), hand_->GetSensorOrder().size());
}

TEST_F(OmniHand3UltraMT1P3Test, CreateHand) {
  EXPECT_NE(hand_, nullptr);
}

TEST_F(OmniHand3UltraMT1P3Test, Init) {
  bool init_result = hand_->Init();
}

TEST_F(OmniHand3UltraMT1P3Test, GetVendorInfo) {
  if (hand_->Init()) {
    auto vendor_info = hand_->GetVendorInfo();
    AgilinkLogger::get().infof(TAG, "[GetVendorInfo] %s", vendor_info.ToString().c_str());
    if (vendor_info.dof == 0) {
      AgilinkLogger::get().warnf(TAG, "[GetVendorInfo] Failed: got empty vendor info (timeout)");
      return;
    }
    const uint8_t doa = hand_->GetDoA();
    EXPECT_EQ(doa, 20);
    EXPECT_EQ(hand_->GetNumOfJointMotors(), doa);
    EXPECT_EQ(hand_->GetRevision(), agilink::omnihand::H3UMRevision::T1P3);
    const auto joint_names = hand_->GetJointNames();
    EXPECT_EQ(joint_names.size(), doa);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, GetDeviceInfo) {
  if (hand_->Init()) {
    auto device_info = hand_->GetDeviceInfo();
    AgilinkLogger::get().infof(TAG, "[GetDeviceInfo] %s", device_info.ToString().c_str());
    if (device_info.hand_device_id != 0) {
      EXPECT_EQ(device_info.hand_device_id, 9);
    }
  }
}

TEST_F(OmniHand3UltraMT1P3Test, GetNonPrivateHandDeviceIdByBroadcast) {
  if (!hand_->Init()) return;

  const uint8_t device_id = hand_->GetNonPrivateHandDeviceIdByBroadcast();
  ASSERT_GT(device_id, 0u) << "No H3U_M replied to the broadcast request";
  EXPECT_EQ(hand_->GetHandDeviceId(), device_id);
}

TEST_F(OmniHand3UltraMT1P3Test, MixControlRejectsInvalidArguments) {
  constexpr size_t kDof = agilink::omnihand::OmniHand3UltraM::kT1P3DegreesOfActiveFreedom;
  const std::vector<int16_t> valid(kDof, 0);
  const std::vector<int16_t> short_values(kDof - 1, 0);

  EXPECT_TRUE(hand_->MixControlByPT(short_values, short_values).empty());
  EXPECT_TRUE(hand_->MixControlByPVT(valid, short_values, valid).empty());

  const auto invalid_pt = hand_->MixControlByPT(0, 0, 0);
  EXPECT_EQ(invalid_pt.ctrl_mode_, 0);
  const auto invalid_pvt = hand_->MixControlByPVT(static_cast<uint8_t>(kDof + 1), 0, 0, 0);
  EXPECT_EQ(invalid_pvt.ctrl_mode_, 0);
}

// TEST_F(OmniHand3UltraMT1P3Test, SetDeviceId) {
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

TEST_F(OmniHand3UltraMT1P3Test, GestureDance) {
  if (!hand_->Init()) return;

  constexpr size_t MOTOR_TOTAL_COUNT = 20;

  std::vector<unsigned char> pp_modes(MOTOR_TOTAL_COUNT, static_cast<unsigned char>(agilink::omnihand::ControlMode::PROFILE_POSITION));
  std::vector<unsigned char> csp_modes(MOTOR_TOTAL_COUNT, static_cast<unsigned char>(agilink::omnihand::ControlMode::POSITION));

  AgilinkLogger::get().infof(TAG, "[GestureDance] Switching to PP mode (PROFILE_POSITION=7)");
  hand_->SetAllControlMode(pp_modes);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // The second gesture intentionally keeps axis 1 at 2048.
  const std::vector<std::vector<int16_t>> gestures = {
      {2048, 512, 512, 512, 2048, 512, 512, 512, 2048, 512,
       512, 512, 2048, 512, 512, 512, 3192, 1024, 512, 512},
      {2048, 4096, 4096, 2048, 2048, 4096, 4096, 2048, 2048, 4096,
       4096, 2048, 2048, 4096, 512, 512, 3156, 1349, 512, 512},
  };

  for (size_t i = 0; i < gestures.size(); ++i) {
    AgilinkLogger::get().infof(TAG, "[GestureDance] Gesture %zu/%zu: %s",
                              i + 1, gestures.size(), ValuesToString(gestures[i]).c_str());
    const auto feedback = hand_->SetAllJointMotorPosi(gestures[i]);
    EXPECT_EQ(feedback.size(), MOTOR_TOTAL_COUNT);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }

  // Repeat one safe single-joint transition using joint 4 (SDK index 4).
  constexpr uint8_t SINGLE_JOINT_INDEX = 4;
  const int16_t single_joint_positions[] = {512, 4096};
  for (const int16_t position : single_joint_positions) {
    const auto feedback = hand_->SetJointMotorPosi(SINGLE_JOINT_INDEX, position);
    AgilinkLogger::get().infof(TAG, "[GestureDance] Joint 4 -> %d (feedback=%d)",
                              static_cast<int>(position), static_cast<int>(feedback));
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }

  AgilinkLogger::get().infof(TAG, "[GestureDance] Switching back to CSP mode (POSITION=0)");
  hand_->SetAllControlMode(csp_modes);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  AgilinkLogger::get().infof(TAG, "[GestureDance] Done!");
}

TEST_F(OmniHand3UltraMT1P3Test, ControlMode) {
  if (hand_->Init()) {
    auto current_modes = hand_->GetAllControlMode();
    if (current_modes.empty() || current_modes.size() != 20) {
      AgilinkLogger::get().warnf(TAG, "[GetAllControlMode] Failed: got %zu modes, expected 20",
                                current_modes.size());
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAllControlMode] Control Modes: %s",
                              ValuesToString(current_modes).c_str());
    EXPECT_EQ(current_modes.size(), 20);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, ErrorReport) {
  if (hand_->Init()) {
    auto error_reports = hand_->GetAllErrorReport();
    if (error_reports.empty()) return;
    std::ostringstream reports;
    for (size_t i = 0; i < error_reports.size(); ++i) {
      if (i > 0) reports << " ";
      reports << "J" << (i + 1) << ":["
              << agilink::omnihand::H3UMErrorReportToString(error_reports[i]) << "]";
    }
    AgilinkLogger::get().infof(TAG, "[GetAllErrorReport] Error Reports (20 joints): %s",
                              reports.str().c_str());
    EXPECT_EQ(error_reports.size(), 20);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, ClearErrorReport) {
  if (hand_->Init()) {
    hand_->ClearAllErrorReport();
    AgilinkLogger::get().infof(TAG, "[ClearAllErrorReport] Done");

    hand_->ClearErrorReport(1);
    AgilinkLogger::get().infof(TAG, "[ClearErrorReport] Joint 1 Done");
  }
}

TEST_F(OmniHand3UltraMT1P3Test, TemperatureReport) {
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
    EXPECT_EQ(temp_reports.size(), 20);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, CurrentReport) {
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
    EXPECT_EQ(current_reports.size(), 20);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, GetAllJointMotorVelo) {
  if (hand_->Init()) {
    auto current_velocities = hand_->GetAllJointMotorVelo();
    if (current_velocities.empty() || current_velocities.size() != 20) {
      AgilinkLogger::get().warnf(TAG, "[GetAllJointMotorVelo] Failed: got %zu velocities, expected 20",
                                current_velocities.size());
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAllJointMotorVelo] Current Velocities: %s",
                              ValuesToString(current_velocities).c_str());
    EXPECT_EQ(current_velocities.size(), 20);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, AxisLimitPos) {
  if (hand_->Init()) {
    auto limits = hand_->GetAxisLimitPos();
    if (limits.empty()) {
      AgilinkLogger::get().warnf(TAG, "[GetAxisLimitPos] Failed: empty result");
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAxisLimitPos] Min Limits (0.1 deg): %s",
                              ValuesToString(limits.min_limits).c_str());
    AgilinkLogger::get().infof(TAG, "[GetAxisLimitPos] Max Limits (0.1 deg): %s",
                              ValuesToString(limits.max_limits).c_str());
    EXPECT_EQ(limits.min_limits.size(), 20);
    EXPECT_EQ(limits.max_limits.size(), 20);
  }
}

TEST_F(OmniHand3UltraMT1P3Test, ActualAxisPos) {
  if (hand_->Init()) {
    auto actual_pos = hand_->GetAllActualAxisPos();
    if (actual_pos.empty()) {
      AgilinkLogger::get().warnf(TAG, "[GetAllActualAxisPos] Failed: empty result");
      return;
    }
    AgilinkLogger::get().infof(TAG, "[GetAllActualAxisPos] Actual Positions (0.1 deg): %s",
                              ValuesToString(actual_pos).c_str());
    EXPECT_EQ(actual_pos.size(), 20);

    auto single = hand_->GetSingleActualAxisPos(1);
    AgilinkLogger::get().infof(TAG, "[GetSingleActualAxisPos] Joint 1: %d", static_cast<int>(single));
  }
}

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
    } else if (arg == "-c" && i + 1 < argc) {
      g_can_interface = argv[i + 1];
      ++i;
      continue;
    } else if (arg == "--revision" && i + 1 < argc) {
      const std::string revision = argv[++i];
      if (revision != "1.3" && revision != "1.5") {
        AgilinkLogger::get().errorf(TAG, "--revision must be 1.3 or 1.5");
        return 1;
      }
      g_revision_t1p5 = revision == "1.5";
      continue;
    } else if (arg == "--device-id" && i + 1 < argc) {
      try {
        size_t consumed = 0;
        const std::string value = argv[++i];
        const int id = std::stoi(value, &consumed);
        if (consumed != value.size() || id < 1 || id > 0x7f) return 1;
        g_device_id = static_cast<uint8_t>(id);
      } catch (const std::exception&) {
        AgilinkLogger::get().errorf(TAG, "--device-id must be an integer in [1, 127]");
        return 1;
      }
      continue;
    } else if (arg == "--help" || arg == "-h") {
      AgilinkLogger::get().infof(TAG, "Usage: %s [-f INTERVAL] [-d DEVICE] [-c CAN_INTERFACE] [--revision 1.3|1.5] [--device-id ID]", argv[0]);
      AgilinkLogger::get().infof(TAG, "  --revision        Revision for timestamp tests (default: 1.3)");
      AgilinkLogger::get().infof(TAG, "  --device-id       Target hand ID (1-127, default: 9); does not write the device ID");
      AgilinkLogger::get().infof(TAG, "  -f INTERVAL       Set CAN request interval (0-100ms, default: 5ms)");
      AgilinkLogger::get().infof(TAG, "  -d DEVICE         Set CAN device type (zlgcan, hcan or socketcan, default: zlgcan)");
      AgilinkLogger::get().infof(TAG, "  -c CAN_INTERFACE  Set SocketCAN interface name (default: can0)");
      return 0;
    }
    gtest_args.push_back(argv[i]);
  }

  int gtest_argc = static_cast<int>(gtest_args.size());
  ::testing::InitGoogleTest(&gtest_argc, gtest_args.data());
  return RUN_ALL_TESTS();
}
