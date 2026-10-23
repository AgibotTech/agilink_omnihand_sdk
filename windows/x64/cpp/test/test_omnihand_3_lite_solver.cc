// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <iterator>
#include <vector>

#include "omnihand/kinematics/omnihand_3_lite/omnihand_3_lite_solver.h"

using namespace agilink::omnihand::h3l;

namespace {
constexpr std::array<double, 4> kRightGestureAngles[] = {
    {0.000, 0.000, 0.000, -0.767},
    {0.000, 0.830, 0.523, 0.546},
    {1.396, 1.025, 0.570, 0.120},
    {1.396, 1.396, 0.000, -0.767},
    {1.396, 1.396, 0.655, -0.242},
    {1.396, 1.396, 0.000, 0.802},
    {0.000, 0.000, 0.827, -0.767},
    {1.151, 1.069, 0.807, -0.767},
    {0.783, 0.694, 0.000, 0.802},
    {0.000, 0.000, 0.456, 0.802},
    {1.396, 0.000, 0.827, -0.767},
    {1.396, 1.396, 0.655, -0.242},
    {1.396, 1.396, 0.655, -0.242},
    {1.396, 0.000, 0.733, 0.396},
    {0.000, 0.830, 0.523, 0.546},
    {0.000, 0.000, 1.055, 0.802},
    {0.957, 0.885, 0.441, 0.592},
    {1.396, 0.000, 0.000, -0.767},
};
}  // namespace

TEST(OmniHand3LiteSolverTest, H3LSPresetsUseSpecifiedAnglesForEachHand) {
  for (bool is_left : {false, true}) {
    OmniHand3LiteSolver solver(is_left, H3LModel::H3L_S);
    for (std::size_t i = 0; i < std::size(kRightGestureAngles); ++i) {
      const auto gesture = static_cast<OmniHand3LiteGesture>(i);
      SCOPED_TRACE(::testing::Message() << "left=" << is_left << " gesture=" << i);
      const auto& preset = kRightGestureAngles[i];
      std::vector<double> angles(preset.begin(), preset.end());
      if (is_left && gesture == OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_BOW) {
        angles = {0.392, 0.357, 0.791, -0.767};
      } else if (is_left && gesture == OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_WAVE) {
        angles = {0.000, 0.000, 1.169, -0.767};
      }
      const auto positions = solver.SetHandGesture(gesture);
      const auto expected = solver.ActiveJointPos2ActuatorInput(angles);
      ASSERT_EQ(positions.size(), 4u);
      EXPECT_EQ(positions, std::vector<int>(expected.begin(), expected.end()));
      const std::vector<int16_t> inputs(positions.begin(), positions.end());
      const auto recovered = solver.ActuatorInput2ActiveJointPos(inputs);
      const auto& calibration = is_left ? OmniHand3LiteSolver::kH3LSLeft
                                        : OmniHand3LiteSolver::kH3LSRight;
      for (std::size_t joint = 0; joint < angles.size(); ++joint) {
        EXPECT_GE(positions[joint], 0);
        EXPECT_LE(positions[joint], 4096);
        const auto& c = calibration[joint];
        EXPECT_GE(angles[joint], c.rad_min);
        EXPECT_LE(angles[joint], c.rad_max);
        const double clamped = std::clamp(angles[joint], c.rad_min, c.rad_max);
        EXPECT_NEAR(recovered[joint], clamped, (c.rad_max - c.rad_min) / 4096 + 1e-9);
      }
    }
  }
}

TEST(OmniHand3LiteSolverTest, NewPresetsAreUnsupportedOnH3LM) {
  for (bool is_left : {false, true}) {
    OmniHand3LiteSolver solver(is_left, H3LModel::H3L_M);
    for (int gesture = 0; gesture < static_cast<int>(std::size(kRightGestureAngles)); ++gesture) {
      EXPECT_TRUE(solver.SetHandGesture(static_cast<OmniHand3LiteGesture>(gesture)).empty());
    }
  }
}

TEST(OmniHand3LiteSolverTest, GestureIdsAndInvalidValues) {
  EXPECT_EQ(static_cast<int>(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OPEN), 0);
  EXPECT_EQ(static_cast<int>(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_OK), 1);
  EXPECT_EQ(static_cast<int>(OmniHand3LiteGesture::OMNI_HAND_3_LITE_GESTURE_NUM8),
            static_cast<int>(std::size(kRightGestureAngles)) - 1);
  EXPECT_EQ(std::size(OmniHand3LiteSolver::kH3LSGestureAngles), std::size(kRightGestureAngles));
  for (auto model : {H3LModel::H3L_S, H3LModel::H3L_M}) {
    for (bool is_left : {false, true}) {
      OmniHand3LiteSolver solver(is_left, model);
      for (int invalid : {std::numeric_limits<int>::min(), -1,
                          static_cast<int>(std::size(kRightGestureAngles)),
                          std::numeric_limits<int>::max()}) {
        EXPECT_TRUE(solver.SetHandGesture(static_cast<OmniHand3LiteGesture>(invalid)).empty());
      }
    }
  }
}
