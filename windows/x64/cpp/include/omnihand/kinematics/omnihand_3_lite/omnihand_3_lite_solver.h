// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

/**
 * @file omnihand_3_lite_solver.h
 * @brief Joint-angle solver for OmniHand 3 Lite S/M (H3L, 4-DOF).
 *
 * Motors 1 and 4 are reversed between left and right hands.
 * H3L-S angle presets use the same angles for both hands, except BOW/WAVE.
 * The calibration tables handle left/right motor direction differences.
 */

#ifndef AGILINK_OMNIHAND_3_LITE_SOLVER_H
#define AGILINK_OMNIHAND_3_LITE_SOLVER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

#include "omnihand/export_symbols.h"
#include "omnihand/utils.h"

namespace agilink {
namespace omnihand {
namespace h3l {
enum class H3LModel : uint8_t {
  H3L_S = 0,
  H3L_M = 1,
};

enum class H3LActivateJoint {
  H3L_ACTIVATE_JOINT_MRP_PIP = 0,
  H3L_ACTIVATE_JOINT_INDEX_PIP,
  H3L_ACTIVATE_JOINT_THUMB_PIP,
  H3L_ACTIVATE_OTHER_THUMB_ABAD,
  H3L_ACTIVATE_JOINT_COUNT
};
enum class OmniHand3LiteGesture : int {
  OMNI_HAND_3_LITE_GESTURE_OPEN = 0,
  OMNI_HAND_3_LITE_GESTURE_OK,
  OMNI_HAND_3_LITE_GESTURE_ONE_HANDED_FINGER_HEART,
  OMNI_HAND_3_LITE_GESTURE_LIKE,
  OMNI_HAND_3_LITE_GESTURE_SIDE_FIST,
  OMNI_HAND_3_LITE_GESTURE_FRONT_FIST,
  OMNI_HAND_3_LITE_GESTURE_PRAYER,
  OMNI_HAND_3_LITE_GESTURE_HAND_HEART1,
  OMNI_HAND_3_LITE_GESTURE_HAND_HEART2,
  OMNI_HAND_3_LITE_GESTURE_HAND_HEART3,
  OMNI_HAND_3_LITE_GESTURE_POINT,
  OMNI_HAND_3_LITE_GESTURE_BOW,
  OMNI_HAND_3_LITE_GESTURE_WAVE,
  OMNI_HAND_3_LITE_GESTURE_NUM1,
  OMNI_HAND_3_LITE_GESTURE_NUM3,
  OMNI_HAND_3_LITE_GESTURE_NUM4,
  OMNI_HAND_3_LITE_GESTURE_NUM7,
  OMNI_HAND_3_LITE_GESTURE_NUM8,
};

class AGIBOT_EXPORT OmniHand3LiteSolver {
 public:
  static constexpr int16_t kActuatorInputMax = 4096;
  static constexpr int16_t kActuatorInputMin = 0;
  static constexpr uint8_t kDegreesOfActiveFreedom = 4;
  static constexpr std::size_t kActiveJointCount = static_cast<std::size_t>(H3LActivateJoint::H3L_ACTIVATE_JOINT_COUNT);
  struct JointMotorCalib {
    double rad_min;        // URDF lower limit (rad).
    double rad_max;        // URDF upper limit (rad).
    int16_t actuator_min;  // Actuator input that corresponds to rad_min.
    int16_t actuator_max;  // Actuator input that corresponds to rad_max.
  };
  explicit OmniHand3LiteSolver(bool is_left_hand);
  OmniHand3LiteSolver(bool is_left_hand, H3LModel model);
  ~OmniHand3LiteSolver() = default;
  // Return actuator inputs; unsupported gestures return an empty vector.
  std::vector<int> SetHandGesture(OmniHand3LiteGesture gesture) const;
  static constexpr Int16Bound GetMotorPositionRange() {
    return {kActuatorInputMin, kActuatorInputMax};
  }

  static constexpr FloatBound GetActivateJointAngleRange(
      H3LActivateJoint joint_index, bool is_left_hand,
      H3LModel model = H3LModel::H3L_S) {
    (void)is_left_hand;
    if (joint_index == H3LActivateJoint::H3L_ACTIVATE_JOINT_COUNT) {
      return {};
    }
    const JointMotorCalib& calib =
        GetCalibrationTable(is_left_hand, model)
            [static_cast<std::size_t>(joint_index)];
    return FloatBound{
      static_cast<float>(calib.rad_min),
      static_cast<float>(calib.rad_max)
    };
  }
  double SingleActuatorInput2ActiveJointPos(std::size_t joint_index, int16_t actuator_input) const;
  int16_t SingleActiveJointPos2ActuatorInput(std::size_t joint_index, double rad) const;
  std::vector<double> ActuatorInput2ActiveJointPos(const std::vector<int16_t>& actuator_input) const;
  std::vector<int16_t> ActiveJointPos2ActuatorInput(const std::vector<double>& active_joint_pos) const;
  static std::vector<double> ConvertActiveJointAngles(
      const std::vector<double>& active_joint_pos, bool source_is_left,
      bool target_is_left);
  std::vector<int16_t> ConvertJointMotorPositions(
      const std::vector<int16_t>& positions, bool source_is_left,
      bool target_is_left) const;
  H3LModel GetModel() const { return model_; }

  using GestureAngles = std::array<double, kActiveJointCount>;
  // H3L-S right-hand angles (rad), in OmniHand3LiteGesture order.
  // Endpoint values are rounded inward to stay within limits.
  // Presets preserve motor targets under the shared H3L-S angle convention.
  // The number of presets is inferred from the initializer.
  inline static constexpr GestureAngles kH3LSGestureAngles[] = {
      {0.000000, 0.000000, 0.000000, 0.000944},  // Open
      {0.000000, 0.830000, 0.523001, 1.313945},  // OK
      {1.396000, 1.025000, 0.570001, 0.887945},  // One-handed finger heart
      {1.396000, 1.396000, 0.000000, 0.000944},  // Like
      {1.396000, 1.396000, 0.655001, 0.525945},  // Side fist
      {1.396000, 1.396000, 0.000000, 1.569945},  // Front fist
      {0.000000, 0.000000, 0.827000, 0.000944},  // Prayer
      {1.151000, 1.069000, 0.807001, 0.000944},  // Two-hand heart 1
      {0.783000, 0.694000, 0.000000, 1.569945},  // Two-hand heart 2
      {0.000000, 0.000000, 0.456001, 1.569945},  // Two-hand heart 3
      {1.396000, 0.000000, 0.827000, 0.000944},  // Point
      {1.396000, 1.396000, 0.655001, 0.525945},  // Bow
      {1.396000, 1.396000, 0.655001, 0.525945},  // Wave
      {1.396000, 0.000000, 0.733001, 1.163945},  // Digit 1
      {0.000000, 0.830000, 0.523001, 1.313945},  // Digit 3
      {0.000000, 0.000000, 1.055001, 1.569945},  // Digit 4
      {0.957000, 0.885000, 0.441000, 1.359945},  // Digit 7
      {1.396000, 0.000000, 0.000000, 0.000944},  // Digit 8
  };
  inline static constexpr GestureAngles kH3LSLeftBowAngles =
      {0.392000, 0.357000, 0.791001, 0.000944};
  inline static constexpr GestureAngles kH3LSLeftWaveAngles =
      {0.000000, 0.000000, 1.169001, 0.000944};

  using JointMotorCalibTable =
      std::array<JointMotorCalib, kActiveJointCount>;

  // Active-joint order: MRP/index PIP, thumb PIP, thumb ABAD/rolling.
  // Six-decimal URDF limits are used directly.
  // Higher-precision limits are rounded inward (ceil lower, floor upper).
  // Each entry is {rad_min, rad_max, actuator_at_rad_min,
  // actuator_at_rad_max}, so the complete linear mapping stays together.
  inline static constexpr JointMotorCalibTable kH3LSRight = {{
      {0.000000, 1.396263, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.396263, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.169371, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.570796, kActuatorInputMax, kActuatorInputMin},
  }};
  inline static constexpr JointMotorCalibTable kH3LSLeft = {{
      {0.000000, 1.396263, kActuatorInputMin, kActuatorInputMax},
      {0.000000, 1.396263, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.169371, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.570796, kActuatorInputMin, kActuatorInputMax},
  }};
  inline static constexpr JointMotorCalibTable kH3LMRight = {{
      {0.000000, 1.570796, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.570796, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 0.907571, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.570796, kActuatorInputMax, kActuatorInputMin},
  }};
  inline static constexpr JointMotorCalibTable kH3LMLeft = {{
      {0.000000, 1.570796, kActuatorInputMin, kActuatorInputMax},
      {0.000000, 1.570796, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 0.907571, kActuatorInputMax, kActuatorInputMin},
      {0.000000, 1.570796, kActuatorInputMin, kActuatorInputMax},
  }};

 private:
  static constexpr const JointMotorCalibTable& GetCalibrationTable(
      bool is_left_hand, H3LModel model) {
    if (model == H3LModel::H3L_M) {
      return is_left_hand ? kH3LMLeft : kH3LMRight;
    }
    return is_left_hand ? kH3LSLeft : kH3LSRight;
  }

  bool is_left_hand_;
  H3LModel model_;
  const JointMotorCalibTable* table_;
};
}  // namespace h3l
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_3_LITE_SOLVER_H
