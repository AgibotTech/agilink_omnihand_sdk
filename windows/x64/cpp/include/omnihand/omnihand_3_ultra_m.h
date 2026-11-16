// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2

/**
 * @file omnihand_3_ultra_m.h
 * @brief OmniHand 3 Ultra M T1.3/T1.5 interface class
 * @note This is the public interface for OmniHand 3 Ultra product
 */

#ifndef AGILINK_OMNIHAND_3_ULTRA_M_H
#define AGILINK_OMNIHAND_3_ULTRA_M_H

// Lets clients retain support for SDK releases without motor discovery and OTA.
#define OMNIHAND_H3UM_MOTOR_OTA_API 6

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "omnihand/export_symbols.h"
#include "omnihand/i_control_mode.h"
#include "omnihand/i_tactile_sensor_3d_i16.h"
#include "omnihand/i_tactile_sensor_1d_u16.h"
#include "omnihand/kinematics/omnihand_3_ultra_m/omnihand_3_ultra_m_solver.h"
#include "omnihand/omnihand.h"
#include "omnihand/proto.h"
#include "omnihand/utils.h"

namespace agilink {
namespace omnihand {

inline const std::map<Finger, size_t> kH3UMT1P3TactilePointCounts = {
    {Finger::ALL_FINGERS, 102},
    {Finger::THUMB, 15},
    {Finger::INDEX, 15},
    {Finger::MIDDLE, 15},
    {Finger::RING, 15},
    {Finger::LITTLE, 15},
    {Finger::PALM, 27},
};

inline const std::map<Finger, size_t> kH3UMT1P5TactilePointCounts = {
    {Finger::ALL_FINGERS, 188},
    {Finger::THUMB, 30},
    {Finger::INDEX, 30},
    {Finger::MIDDLE, 30},
    {Finger::RING, 30},
    {Finger::LITTLE, 30},
    {Finger::PALM, 38},
};

// ============================================
// Xense Visual-Tactile Sensor Interface
// ============================================
// Xense frame data structure
struct AGIBOT_EXPORT XenseFrame {
  // Rectified image (BGR, HWC format, uint8)
  std::vector<uint8_t> rectify_image;
  uint32_t rectify_width = 0;
  uint32_t rectify_height = 0;

  // Difference image (rectify - reference, uint8, HWC)
  std::vector<uint8_t> difference_image;
  uint32_t difference_width = 0;
  uint32_t difference_height = 0;

  // Force distribution (float32, row-major, shape [H, W, C])
  // C=3 for 3D force vector at each pixel
  std::vector<float> force;
  uint32_t force_width = 0;
  uint32_t force_height = 0;

  // 6-DOF resultant force/torque [Fx, Fy, Fz, Tx, Ty, Tz]
  float force_resultant[6] = {0, 0, 0, 0, 0, 0};

  // Depth map (float32, meters, shape [H, W])
  std::vector<float> depth;
  uint32_t depth_width = 0;
  uint32_t depth_height = 0;

  // Initial mesh 3D coordinates (float32, shape [H, W, 3], mm)
  std::vector<float> mesh3d_init;
  uint32_t mesh_width = 0;
  uint32_t mesh_height = 0;

  // Mesh displacement field (float32, shape [H, W, 3], mm)
  std::vector<float> mesh3d_flow;

  // Timestamp (nanoseconds)
  uint64_t timestamp = 0;
};

/// Convert raw depth map (float32, meters) to RGB uint8 image (HWC).
/// Auto-normalizes to [0,1] per-frame, then applies color mapping.
/// Colors: gray → blue → cyan → green → orange.
AGIBOT_EXPORT void DepthToRGB(const std::vector<float>& depth,
                               uint32_t width, uint32_t height,
                               std::vector<uint8_t>& rgb);

// Note: Palm is NOT Xense-based — it uses a separate data source (TCP from SoC)
struct AGIBOT_EXPORT PalmFrame {
  // Force distribution (float32, interleaved fx,fy,fz per taxel, raw int16 units)
  std::vector<float> force;
  uint32_t width = 0;
  uint32_t height = 0;
  // Server-computed resultant [Fx, Fy, Fz] (same int16 units)
  float resultant[3] = {0, 0, 0};
  double timestamp = 0.0;
};

// All tactile data: 5 fingers + palm
struct AGIBOT_EXPORT AllTactileFrame {
  XenseFrame fingers[5];
  PalmFrame palm;
};

enum class H3UMErrorBit : uint16_t {
  H3U_M_ERR_ENCODER_COMM_TIMEOUT = 1 << 0,
  H3U_M_ERR_CALIBRATION          = 1 << 1,
  H3U_M_ERR_OVER_VOLTAGE         = 1 << 2,
  H3U_M_ERR_UNDER_VOLTAGE        = 1 << 3,
  H3U_M_ERR_OVER_TEMPERATURE     = 1 << 4,
  H3U_M_ERR_TORQUE_SATURATION    = 1 << 5,
  H3U_M_ERR_PARAM_CRC            = 1 << 6,
  H3U_M_ERR_HOMING               = 1 << 7,
  H3U_M_ERR_POSITION_FOLLOWING   = 1 << 8,
  H3U_M_ERR_VELOCITY_FOLLOWING   = 1 << 9,
  H3U_M_ERR_OVER_CURRENT         = 1 << 10,
  H3U_M_ERR_INNER_ENCODER_CRC    = 1 << 11,
  H3U_M_ERR_OUTER_ENCODER_CRC    = 1 << 12,
  H3U_M_ERR_ENCODER_MULTI_TURN   = 1 << 13,
  H3U_M_ERR_ANGLE_IDENTIFY_FAIL  = 1 << 14,
  H3U_M_ERR_RESERVED             = 1 << 15,
};

inline std::string H3UMErrorReportToString(const JointMotorErrorReport& report) {
  if (report.value_ == 0) return "0";
  static const char* names[] = {
      "encoder_comm_timeout",
      "calibration_error",
      "over_voltage",
      "under_voltage",
      "over_temperature",
      "torque_saturation",
      "param_crc_error",
      "homing_error",
      "position_following_error",
      "velocity_following_error",
      "over_current",
      "inner_encoder_crc_error",
      "outer_encoder_crc_error",
      "encoder_multi_turn_error",
      "angle_identify_fail",
      "bi15_reserved",
  };
  std::string s;
  for (int i = 0; i < 16; ++i) {
    if (report.value_ & (1 << i)) {
      if (!s.empty()) s += ",";
      s += names[i];
    }
  }
  return s;
}

/**
 * @brief OmniHand 3 Ultra M T1.3/T1.5 interface class
 *
 * This class provides the public interface for OmniHand 3 Ultra product.
 * It supports 20 active degrees of freedom and optional Xense visual-tactile sensor.
 *
 * Angle <-> motor position convention:
 *   - SetAllJointMotorPosi / GetAllJointMotorPosi int16_t values range [0, 4095]
 *     (12-bit, unrelated to calibration register Pn7/Pn8 0.1-degree encoding).
 *   - URDF active joint angle (rad) and tick are **per-joint independent linear mappings**,
 *     with different coefficients per joint that may be reversed;
 *     H3UM does not need the polynomial correction used in O10.
 *   - For actual conversion and calibration table, see
 *       kinematics/omnihand_3_ultra_m/omnihand_3_ultra_m_solver.{h,cc}
 *     SetAll/GetAllActiveJointAngles internally forwards to OmniHand3UltraMSolver,
 *     so upper layers (including ROS2 nodes / Python bindings) only need to use rad units.
 *
 * TODO(H3UM): Kinematics solver (including passive joints) not yet integrated:
 *   SetHandGesture, and interfaces for accessing passive joints by single joint
 *   name/index are still placeholder implementations.
 */
class AGIBOT_EXPORT OmniHand3UltraM : public OmniHand,
                                      public IControlMode {
 public:
  // Constants
  static constexpr unsigned char kT1P3DegreesOfActiveFreedom = 20;
  static constexpr unsigned char kT1P5DegreesOfActiveFreedom = 21;
  static constexpr Int16Range kT1P3MixCtrlTorqueRange = {0, 10000, 1000};  // mA
  static constexpr Int16Range kT1P5MixCtrlTorqueRange = {0, 2000, 1900};  // mA
  static constexpr uint8_t kDegreesOfPassiveFreedom = 0; // DoP
  static constexpr uint8_t kT1P3DefaultHandDeviceId = 9u;
  static constexpr uint8_t kT1P5DefaultHandDeviceId = 1u;
  static constexpr char kDefaultTactileSocIp[] = "192.168.99.2";
  static constexpr uint16_t kDefaultTactileSocPort = 19009;

  static constexpr uint8_t GetDefaultHandDeviceId(H3UMRevision revision) {
    return revision == H3UMRevision::T1P5
        ? kT1P5DefaultHandDeviceId
        : kT1P3DefaultHandDeviceId;
  }

  virtual ~OmniHand3UltraM() = default;

  // ============ Factory Methods ============
  // Address discovery assumes one hand on the selected CAN channel.
  // Without a broadcast reply, discovery tries kT1P3DefaultHandDeviceId.
  /**
   * @brief Factory method - CAN communication (ZLG USB CANFD) by canfd_device_id
   * @note Automatically detects T1.3/T1.5 from the reported active DOF.
   * @param hand_type Hand type (left/right)
   * @param hand_device_id 0 broadcasts to discover the device ID; nonzero skips broadcast.
   * @param canfd_device_id CANFD device ID
   * @param canfd_channel_id CANFD channel ID
   * @param tactile_soc_ip Tactile SoC IP for palm and Xense (empty = disabled)
   * @param tactile_soc_port Tactile SoC data port
   * @return A hand instance, or nullptr if connection or revision detection fails.
   */
  static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
      HandType hand_type,
      uint8_t hand_device_id,
      uint8_t canfd_device_id,
      uint8_t canfd_channel_id = 0,
      const std::string& tactile_soc_ip = kDefaultTactileSocIp,
      uint16_t tactile_soc_port = kDefaultTactileSocPort,
      CommunicationLogOptions log_options = {});

  static std::unique_ptr<OmniHand3UltraM> createHandT1P5ByZlgcan(
      HandType hand_type,
      uint8_t hand_device_id,
      uint8_t canfd_device_id,
      uint8_t canfd_channel_id = 0,
      CommunicationLogOptions log_options = {});

  /**
   * @brief Factory method - CAN communication (ZLG USB CANFD) by serial number
   * @note Automatically detects T1.3/T1.5 from the reported active DOF.
   * @param hand_type Hand type (left/right)
   * @param hand_device_id 0 broadcasts to discover the device ID; nonzero skips broadcast.
   * @param usbcanfd_serial_number USB CANFD serial number
   * @param canfd_channel_id CANFD channel ID
   * @param tactile_soc_ip Tactile SoC IP for palm and Xense (empty = disabled)
   * @param tactile_soc_port Tactile SoC data port
   * @return A hand instance, or nullptr if connection or revision detection fails.
   */
  static std::unique_ptr<OmniHand3UltraM> createHandByZlgcan(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& usbcanfd_serial_number,
      uint8_t canfd_channel_id = 0,
      const std::string& tactile_soc_ip = kDefaultTactileSocIp,
      uint16_t tactile_soc_port = kDefaultTactileSocPort,
      CommunicationLogOptions log_options = {});

  static std::unique_ptr<OmniHand3UltraM> createHandT1P5ByZlgcan(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& usbcanfd_serial_number,
      uint8_t canfd_channel_id = 0,
      CommunicationLogOptions log_options = {});

#if OMNIHAND_ZLG_TCP_SUPPORTED
  /**
   * @brief Factory method - ZLG CANFD over TCP (WiFi/Ethernet to CANFD, this machine acts as TCP client)
   * @note Only available on Windows and Linux x64 (not supported on Linux aarch64/arm64)
   * @note Automatically detects T1.3/T1.5 from the reported active DOF.
   * @param hand_type Hand type (left/right)
   * @param hand_device_id 0 broadcasts to discover the device ID; nonzero skips broadcast.
   * @param tcp_host TCP server IP or hostname (e.g. "192.168.0.178")
   * @param tcp_port TCP server port (e.g. 8000)
   * @param canfd_channel_id CAN channel index (0 or 1, default 0)
   * @param tactile_soc_ip Tactile SoC IP for palm and Xense (empty = disabled)
   * @param tactile_soc_port Tactile SoC data port
   * @return A hand instance, or nullptr if connection or revision detection fails.
   */
  static std::unique_ptr<OmniHand3UltraM> createHandByZlgCanTcp(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& tcp_host,
      uint16_t tcp_port,
      uint8_t canfd_channel_id = 0,
      const std::string& tactile_soc_ip = kDefaultTactileSocIp,
      uint16_t tactile_soc_port = kDefaultTactileSocPort,
      CommunicationLogOptions log_options = {});

  static std::unique_ptr<OmniHand3UltraM> createHandT1P5ByZlgCanTcp(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& tcp_host,
      uint16_t tcp_port,
      uint8_t canfd_channel_id = 0,
      CommunicationLogOptions log_options = {});
#endif

#ifdef __linux__
  /**
   * @brief Factory method - SocketCAN communication (Linux native CAN interface)
   * @note Automatically detects T1.3/T1.5 from the reported active DOF.
   * @param hand_type Hand type (left/right)
   * @param hand_device_id 0 broadcasts to discover the device ID; nonzero skips broadcast.
   * @param can_interface CAN interface name (e.g. "can0")
   * @param tactile_soc_ip Tactile SoC IP for palm and Xense (empty = disabled)
   * @param tactile_soc_port Tactile SoC data port
   * @return A hand instance, or nullptr if connection or revision detection fails.
   */
  static std::unique_ptr<OmniHand3UltraM> createHandSocketCan(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& can_interface = "can0",
      const std::string& tactile_soc_ip = kDefaultTactileSocIp,
      uint16_t tactile_soc_port = kDefaultTactileSocPort,
      CommunicationLogOptions log_options = {});

  static std::unique_ptr<OmniHand3UltraM> createHandT1P5SocketCan(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& can_interface = "can0",
      CommunicationLogOptions log_options = {});
#endif

  /**
   * @brief Factory method - HCAN USB CANFD communication (by canfd_device_id)
   * @note Automatically detects T1.3/T1.5 from the reported active DOF.
   * @param hand_type Hand type (left/right)
   * @param hand_device_id 0 broadcasts to discover the device ID; nonzero skips broadcast.
   * @param canfd_device_id CANFD device ID
   * @param canfd_channel_id CANFD channel ID
   * @param tactile_soc_ip Tactile SoC IP for palm and Xense (empty = disabled)
   * @param tactile_soc_port Tactile SoC data port
   * @return A hand instance, or nullptr if connection or revision detection fails.
   */
  static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
      HandType hand_type,
      uint8_t hand_device_id,
      uint8_t canfd_device_id,
      uint8_t canfd_channel_id = 0,
      const std::string& tactile_soc_ip = kDefaultTactileSocIp,
      uint16_t tactile_soc_port = kDefaultTactileSocPort,
      CommunicationLogOptions log_options = {});

  static std::unique_ptr<OmniHand3UltraM> createHandT1P5ByHcan(
      HandType hand_type,
      uint8_t hand_device_id,
      uint8_t canfd_device_id,
      uint8_t canfd_channel_id = 0,
      CommunicationLogOptions log_options = {});

  /**
   * @brief Factory method - HCAN USB CANFD communication (by serial number)
   * @note Automatically detects T1.3/T1.5 from the reported active DOF.
   * @param hand_type Hand type (left/right)
   * @param hand_device_id 0 broadcasts to discover the device ID; nonzero skips broadcast.
   * @param hcan_serial_number HCAN serial number
   * @param canfd_channel_id CANFD channel ID
   * @param tactile_soc_ip Tactile SoC IP for palm and Xense (empty = disabled)
   * @param tactile_soc_port Tactile SoC data port
   * @return A hand instance, or nullptr if connection or revision detection fails.
   */
  static std::unique_ptr<OmniHand3UltraM> createHandByHcan(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& hcan_serial_number,
      uint8_t canfd_channel_id = 0,
      const std::string& tactile_soc_ip = kDefaultTactileSocIp,
      uint16_t tactile_soc_port = kDefaultTactileSocPort,
      CommunicationLogOptions log_options = {});

  static std::unique_ptr<OmniHand3UltraM> createHandT1P5ByHcan(
      HandType hand_type,
      uint8_t hand_device_id,
      const std::string& hcan_serial_number,
      uint8_t canfd_channel_id = 0,
      CommunicationLogOptions log_options = {});

  virtual uint8_t GetDefaultNonPrivateHandDeviceId() const = 0;

  int GetHandDeviceIdByBroadcast() override;

  // ============ Joint Naming ============
  /**
   * @brief Returns the joint names in motor-index order (20 for T1.3, 21 for T1.5).
   * @note Order MUST match SetAllJointMotorPosi / GetAllJointMotorPosi index layout.
   *       This is the single source of truth for "motor index <-> ROS joint name"
  *       binding; ROS2 node code should call this instead of hard-coding the list.
  */
  std::vector<std::string> GetJointNames() const override {
    return revision_ == H3UMRevision::T1P5
        ? h3um::OmniHand3UltraMSolver::kT1P5JointNames
        : h3um::OmniHand3UltraMSolver::kT1P3JointNames;
  }

  uint8_t GetNumOfJointMotors() const override {
    return static_cast<uint8_t>(revision_);
  }

  uint8_t GetDoA() const override {
    return static_cast<uint8_t>(revision_);
  }

  uint8_t GetDoP() const override {
    return kDegreesOfPassiveFreedom;
  }

  std::vector<double> GetAllJointAnglesFromMotorPositions(
      const std::vector<int16_t>& motor_positions) const override;

  std::vector<double> ConvertActiveJointAngles(
      const std::vector<double>& angles, HandType source_hand,
      HandType target_hand) const override;

  std::vector<int16_t> ConvertJointMotorPositions(
      const std::vector<int16_t>& positions, HandType source_hand,
      HandType target_hand) const override;

  H3UMRevision GetRevision() const {
    return revision_;
  }

  static constexpr Int16Bound GetMinMaxMotorPosition(uint8_t joint_motor_index) {
    (void)joint_motor_index;
    return h3um::OmniHand3UltraMSolver::GetMotorPositionRange();
  }

  static constexpr Int16Bound GetMinMaxActualMotorPosition(uint8_t joint_motor_index) {
    (void)joint_motor_index;
    return kActualMotorPositionBound;
  }

  Int16Range GetMinMaxDefaultMixCtrlTorque(uint8_t joint_motor_index) const {
    if (joint_motor_index == 0 ||
        joint_motor_index > GetNumOfJointMotors()) {
      return {0, 0, 0};
    }
    return revision_ == H3UMRevision::T1P5
        ? kT1P5MixCtrlTorqueRange
        : kT1P3MixCtrlTorqueRange;
  }

  // ============ Gesture Control ============
  std::vector<int16_t> SetHandGesture(h3um::H3UMGesture gesture);

  std::vector<int16_t> GetHandGesture(h3um::H3UMGesture gesture);

  std::vector<int16_t> SetHandGesture(int gesture_num = 1) override;

  std::vector<int16_t> GetHandGesture(int gesture_num) override;

  // Borrowed interfaces live as long as this hand; unsupported dimensions return nullptr.
  ITactileSensor1DU16* GetTactileSensor1D() {
    return dynamic_cast<ITactileSensor1DU16*>(this);
  }
  const ITactileSensor1DU16* GetTactileSensor1D() const {
    return dynamic_cast<const ITactileSensor1DU16*>(this);
  }
  ITactileSensor3DI16* GetTactileSensor3D() {
    return dynamic_cast<ITactileSensor3DI16*>(this);
  }
  const ITactileSensor3DI16* GetTactileSensor3D() const {
    return dynamic_cast<const ITactileSensor3DI16*>(this);
  }

  // T1.3 exposes XYZ data; T1.5 exposes scalar pressure in grams.
  // InitTactilePointsMap() refreshes point counts from device metadata.
  // T1.5: 1 = Jingzhigan, 2 = HuaweiKe; 0 = unknown or unsupported.
  virtual uint8_t GetTactileSensorType() const {
    return 0;
  }
  virtual size_t GetNumOfTactileSensors() const = 0;
  virtual size_t GetNumOfTactilePoints(Finger sensor) const = 0;
  virtual bool InitTactilePointsMap() = 0;
  virtual size_t GetLenOfTactileDatum(Finger sensor) const = 0;
  virtual size_t GetNumOfRepliedTactileFrames(Finger sensor) const = 0;
  virtual std::string GetSNOfTactileSensor(Finger sensor) const = 0;
  virtual size_t GetSensorDataLength(Finger sensor) const = 0;
  virtual const std::vector<Finger>& GetSensorOrder() const;

  // ============ Xense Tactile Sensing ============
  // Cached frames retain their first SDK host receive time; vendor clocks are unchanged.
  TimestampedResult<bool> GetFingerTactileWithTimestamp(uint8_t finger_index, XenseFrame& frame);
  TimestampedResult<bool> GetPalmTactileWithTimestamp(PalmFrame& frame);
  TimestampedResult<bool> GetAllTactileWithTimestamp(AllTactileFrame& frame);

  /**
   * @brief Get tactile data for a specific finger
   * @param finger_index 0=thumb, 1=index, 2=middle, 3=ring, 4=pinky
   * @param frame Output frame data
   * @return true if frame was successfully retrieved
   */
  virtual bool GetFingerTactile(uint8_t finger_index, XenseFrame& frame) { (void)finger_index; (void)frame; return false; }

  /**
   * @brief Get palm tactile data (TCP from SoC, not Xense)
   * @param frame Output frame (force field filled)
   * @return true if frame was successfully retrieved
   */
  virtual bool GetPalmTactile(PalmFrame& frame) { (void)frame; return false; }

  /**
   * @brief Get all 5 finger tactile data at once
   * @param fingers Output array of 5 finger frames (thumb..pinky)
   * @return true if all frames were successfully retrieved
   */
  virtual bool GetAllTactile(AllTactileFrame& frame) { (void)frame; return false; }

  // ============ T1.5 Extended API ============
  // Synchronous Eyou motor upgrade, currently supported over T1.5 CAN only.
  // Owns pass-through entry and cleanup on success, failure and exceptions.
  // Suspend other hand requests until this returns. Do not reenter hand
  // methods from progress callbacks. Progress counts acknowledged 1KB blocks;
  // errors use SDK OtaErrorCode values, not the motor's raw status byte.
  // Success requires all block CRC replies, App detection and exit command delivery.
  // Returns the confirmed App version on success, or empty on failure.
  // An interrupted upgrade can be restarted from offset 0 when the motor is in Boot.
  // Wait 0~60000 ms after each pass-through switch; default 180 ms.
  virtual std::string UpdateJointMotorFirmware(
      uint8_t node_id, const std::string& file_name,
      OtaProgressCallback progress_callback = nullptr,
      int pass_through_wait_ms = 180) {
    (void)node_id;
    (void)file_name;
    (void)pass_through_wait_ms;
    if (progress_callback) {
      progress_callback(static_cast<int>(OtaErrorCode::AGILINK_OTA_NOT_SUPPORTED),
                        0, OtaProgressStatus::AGILINK_OTA_ERROR);
    }
    return {};
  }

  // Read register 0x28 for motor indices 1~21; returns empty on failure or unsupported revision.
  virtual std::string GetMotorVersion(uint8_t joint_motor_index) const {
    (void)joint_motor_index;
    return {};
  }

  /**
   * @brief Reads one motor's detailed diagnostic state through register 0x81.
   * @param joint_motor_index Motor index, 1~21; 0 (all motors) is not supported.
   * @param info Output in protocol units; cleared on failure.
   * @return true for a complete reply; false for an invalid index, unsupported
   *         revision, communication failure, or a reply shorter than 31 bytes.
   */
  virtual bool GetMotorDiagnosticInfo(uint8_t joint_motor_index,
                                      H3UMMotorDiagnosticInfo& info) const {
    (void)joint_motor_index;
    info = {};
    return false;
  }

  TimestampedResult<int16_t> GetTemperatureOfPalmWithTimestamp() const;
  TimestampedResult<int16_t> GetTemperatureOfPowerWithTimestamp() const;


  /**
   * @brief Gets the palm board temperature through register 0x21/0x51.
   * @return Palm board temperature in degrees Celsius.
   */
  virtual int16_t GetTemperatureOfPalm() const = 0;

  /**
   * @brief Gets the power interface board temperature through register 0x21/0x52.
   * @return Power interface board temperature in degrees Celsius.
   */
  virtual int16_t GetTemperatureOfPower() const = 0;

  // Clear error report
  virtual void ClearAllErrorReport() {}
  virtual void ClearErrorReport(unsigned char joint_motor_index) { (void)joint_motor_index; }

  // Append new virtual APIs to preserve the slots of existing declarations.
  // Scan global nodes 1~21 over T1.5 CAN; owns pass-through entry and cleanup.
  // Attempts cleanup before returning or propagating a callback exception.
  // Each valid reply invokes callback(node_id, in_boot, version) synchronously.
  // Probe Boot first, then CANopen on no valid Boot reply; absent nodes are skipped.
  // Returns true after a completed scan and exit delivery, even with no nodes.
  // Suspend other hand requests until this returns.
  // Callbacks must not reenter hand methods.
  // Callback exceptions propagate to the caller. No Python binding.
  // Wait 0~60000 ms after each pass-through switch; default 20 ms.
  virtual bool ScanJointMotorFirmware(
      std::function<void(uint8_t node_id, bool in_boot,
                         const std::string& version)> callback,
      int pass_through_wait_ms = 20) {
    (void)callback;
    (void)pass_through_wait_ms;
    return false;
  }

 protected:
  OmniHand3UltraM() = default;
  explicit OmniHand3UltraM(std::shared_ptr<DeviceContext> context) : OmniHand(std::move(context)) {}

  /**
   * @brief Initialize base class members and per-joint rad <-> tick solver.
   * @param device_id Device ID
   * @param hand_type Hand type (left/right)
   * @note Product type is fixed to ProductType::OMNIHAND_3_ULTRA_M for this class.
   *       Solver is built here so derived impls (CAN / Serial / ...) can use
   *       joint_motor_solver_ directly without re-implementing init logic.
   */
  void Reset(uint8_t device_id, HandType hand_type, H3UMRevision revision);
  void OnHandTypeChanged() override;

  /**
   * @brief Per-joint linear rad <-> motor-tick solver for OmniHand 3 Ultra.
   *        Bound to kLeft / kRight inside Reset() based on hand_type_.
   *        Shared by every transport implementation.
   */
  std::unique_ptr<h3um::OmniHand3UltraMSolver> joint_motor_solver_;
  H3UMRevision revision_{H3UMRevision::T1P3};
  std::optional<h3um::H3UMGesture> previous_gesture_;
  std::map<Finger, size_t> tactile_points_map_;
  static constexpr Int16Bound kActualMotorPositionBound = {-1800, 1800};
};

}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_3_ULTRA_M_H
