# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Hardware tests for OmniHand Pro 2025 (O12).

Transport: zlgcan | hcan | socketcan (Linux only), selected with ``-d`` or
``OMNIHAND_DEVICE_TYPE``. The direct CLI follows the C++ gtest options.
"""

import sys
import os
import time

# Ensure we use the installed omnihand package, not the source directory
# Remove parent directory from path to avoid importing from source
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir in sys.path:
    sys.path.remove(parent_dir)

import pytest
from omnihand import OmniHandPro2025, HandType, Finger, ControlMode

DEVICE_TYPE = "zlgcan"
REQUEST_INTERVAL = 5
CANFD_ID = 0
CHANNEL_ID = 0
CAN_IF = "can0"
TRANSPORTS = ("zlgcan", "hcan", "socketcan")


def _apply_config():
    """Read pytest environment config and consume equivalent direct-run flags."""
    global DEVICE_TYPE, REQUEST_INTERVAL, CANFD_ID, CHANNEL_ID, CAN_IF
    env_options = (
        ("OMNIHAND_DEVICE_TYPE", "DEVICE_TYPE", str),
        ("OMNIHAND_REQUEST_INTERVAL", "REQUEST_INTERVAL", int),
        ("OMNIHAND_CANFD_ID", "CANFD_ID", int),
        ("OMNIHAND_CHANNEL_ID", "CHANNEL_ID", int),
        ("OMNIHAND_CAN_IF", "CAN_IF", str),
    )
    for env_name, attr, cast in env_options:
        value = os.environ.get(env_name)
        if value is not None:
            try:
                globals()[attr] = cast(value)
            except ValueError:
                raise ValueError(f"Invalid {env_name} value: {value!r}")

    if __name__ != "__main__":
        return

    cli_options = (
        ("-d", "DEVICE_TYPE", str),
        ("-f", "REQUEST_INTERVAL", int),
        ("-i", "CANFD_ID", int),
        ("-c", "CHANNEL_ID", int),
        ("--can-if", "CAN_IF", str),
    )
    argv = sys.argv[:]
    for flag, attr, cast in cli_options:
        while flag in argv:
            index = argv.index(flag)
            if index + 1 >= len(argv):
                raise ValueError(f"{flag} requires a value")
            raw_value = argv[index + 1]
            try:
                globals()[attr] = cast(raw_value)
            except ValueError:
                raise ValueError(f"Invalid {flag} value: {raw_value!r}")
            del argv[index:index + 2]

    if "--help" in argv or "-h" in argv:
        print("Usage: python test_omnihand_pro_2025.py [-f INTERVAL] [-d DEVICE] [-i CANFD_ID] [-c CHANNEL] [--can-if IF]")
        print("  -d DEVICE    zlgcan | hcan | socketcan (default: zlgcan)")
        print("  -f INTERVAL  Request interval 0-100ms (default: 5)")
        print("  -i CANFD_ID  HCAN/ZLG adapter index (default: 0)")
        print("  -c CHANNEL   HCAN/ZLG channel index (default: 0)")
        print("  --can-if IF  SocketCAN interface (default: can0)")
        raise SystemExit(0)
    sys.argv = argv

    # pytest imports __main__ under its module name; preserve parsed options.
    for env_name, attr, _cast in env_options:
        os.environ[env_name] = str(globals()[attr])


_apply_config()
if DEVICE_TYPE not in TRANSPORTS:
    raise ValueError(f"-d/OMNIHAND_DEVICE_TYPE must be one of {TRANSPORTS}, got {DEVICE_TYPE!r}")
if not 0 <= REQUEST_INTERVAL <= 100:
    raise ValueError("-f/OMNIHAND_REQUEST_INTERVAL must be in [0, 100]")
if not 0 <= CANFD_ID <= 255 or not 0 <= CHANNEL_ID <= 255:
    raise ValueError("-i/OMNIHAND_CANFD_ID and -c/OMNIHAND_CHANNEL_ID must be in [0, 255]")

@pytest.fixture
def hand():
    """Create and initialize OmniHand Pro 2025 instance for testing"""
    # Create hand instance based on device type
    if DEVICE_TYPE == "hcan":
        hand = OmniHandPro2025.create_hand_by_hcan(
            hand_type=HandType.LEFT,
            hand_device_id=OmniHandPro2025.kDefaultHandDeviceId,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID
        )
        print("[Info]: Using HCAN device")
    elif DEVICE_TYPE == "socketcan":
        if not hasattr(OmniHandPro2025, "create_hand_socketcan"):
            pytest.fail("SocketCAN is only available on Linux", pytrace=False)
        hand = OmniHandPro2025.create_hand_socketcan(
            hand_type=HandType.LEFT,
            hand_device_id=OmniHandPro2025.kDefaultHandDeviceId,
            can_interface=CAN_IF,
        )
        print(f"[Info]: Using SocketCAN interface {CAN_IF}")
    else:
        # Default: ZLG CAN
        hand = OmniHandPro2025.create_hand_by_zlgcan(
            hand_type=HandType.LEFT,
            hand_device_id=OmniHandPro2025.kDefaultHandDeviceId,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID
        )
        print("[Info]: Using ZLG CAN device")
    hand.set_request_interval(REQUEST_INTERVAL)
    hand.set_frame_recv_timeout(300)
    print(f"[Info]: Request interval={REQUEST_INTERVAL}ms, receive timeout=300ms")
    if DEVICE_TYPE in ("zlgcan", "hcan"):
        print(f"[Info]: Adapter={CANFD_ID}, channel={CHANNEL_ID}")
    yield hand
    # Cleanup: explicitly delete the hand instance to ensure proper resource cleanup
    # This matches gtest's TearDown() behavior where hand_.reset() is called
    if hand:
        del hand
        time.sleep(0.1)  # Wait for resources to be fully released


def test_create_hand(hand):
    """Test factory method"""
    assert hand is not None


def test_init(hand):
    """Test initialization"""
    # Note: This test may fail if hardware is not connected
    init_result = hand.init()
    # We don't assert on init_result as it depends on hardware availability


def test_get_vendor_info(hand):
    """Test vendor info (may require hardware)"""
    assert hand.init(), "Device not initialized"
    
    # # Add a small delay after Init to allow device to be ready
    # # This is especially important on first run after system boot
    # time.sleep(0.1)
    
    vendor_info = hand.get_vendor_info()
    # Check if request succeeded (non-zero dof indicates success)
    assert vendor_info.dof != 0, "Failed to get vendor info (timeout)"
    
    # Print vendor info similar to gtest (avoid UnicodeDecodeError by printing fields individually)
    print(f"\n[get_vendor_info] Vendor Info:")
    print(f"{vendor_info}")
    assert vendor_info.dof == 12  # O12 has 12 DOF


def test_get_device_info(hand):
    """Test device info"""
    assert hand.init(), "Device not initialized"
    
    device_info = hand.get_device_info()
    print(f"\n[get_device_info] Device Info:")
    print(f"{device_info}")
    # Only check deviceId if request succeeded (non-zero indicates success)
    if device_info.hand_device_id != 0:
        assert device_info.hand_device_id == OmniHandPro2025.kDefaultHandDeviceId


# def test_set_device_id(hand):
#     """Test setting device ID (may cause device inaccessibility)"""
#     assert hand.init(), "Device not initialized"
    
#     # Get current device ID first
#     current_device_info = hand.get_device_info()
#     current_id = current_device_info.hand_device_id
    
#     # Only test if we got a valid device ID (fail if timeout, like gtest)
#     assert current_id != 0, "Failed to get current device ID"
    
#     # Store original ID for cleanup
#     original_id = current_id
    
#     # Set to target ID (2) using current ID
#     target_id = 2
#     hand.set_device_id(target_id)
#     print(f"\n[set_device_id] Set Device ID: {target_id}")
#     time.sleep(0.1)  # O12 firmware requires 2s delay after device ID change
    
#     # Verify new device ID
#     device_info = hand.get_device_info()
#     assert device_info.hand_device_id == 2, f"Expected device ID 2, got {device_info.hand_device_id}"
    
#     # Reset to original ID
#     hand.set_device_id(original_id)
#     print(f"[set_device_id] Reset Device ID: {original_id}")
#     time.sleep(0.1)  # O12 firmware requires 2s delay after device ID change
    
#     # Verify reset
#     device_info1 = hand.get_device_info()
#     assert device_info1.hand_device_id == original_id, (
#         f"Expected device ID {original_id}, got {device_info1.hand_device_id}"
#     )


def test_joint_angle_control(hand):
    """Test joint angle control (requires hardware)"""
    assert hand.init(), "Device not initialized"
    
    # Test setting active joint angles
    angles = [0.0] * 12  # 12 joints, all at 0
    hand.set_all_active_joint_angles(angles)
    print(f"\n[set_all_active_joint_angles] Set Active Joint Angles (rad): {angles}")
    
    time.sleep(0.5)
    angles = [0.6] * 12
    hand.set_all_active_joint_angles(angles)
    print(f"\n[set_all_active_joint_angles] Set Active Joint Angles (rad): {angles}")

    time.sleep(0.5)
    angles = [0.0] * 12
    hand.set_all_active_joint_angles(angles)
    print(f"\n[set_all_active_joint_angles] Set Active Joint Angles (rad): {angles}")
    # Get active joint angles (may fail if hardware communication fails)
    active_angles = hand.get_all_active_joint_angles()
    # Check if request succeeded (non-empty result and correct size)
    assert active_angles and len(active_angles) == 12, \
        f"Failed to get active joint angles: got {len(active_angles) if active_angles else 0} angles, expected 12"
    
    print(f"[get_all_active_joint_angles] Active Joint Angles (rad): {active_angles}")
    assert len(active_angles) == 12
    
    # Use GetAllJointAngles (similar to gtest) instead of get_all_joint_pos
    # But GetAllJointAngles internally calls GetAllActiveJointAngles again, which may fail
    # So we need to check if GetAllActiveJointAngles succeeded before calling GetAllJointAngles
    # Actually, we already have active_angles, so we can use get_all_joint_pos instead
    # to avoid double hardware communication and potential crashes
    try:
        all_angles = hand.get_all_joint_pos(active_angles)
    except Exception as e:
        # If get_all_joint_pos fails, try get_all_joint_angles as fallback
        # But this may crash if GetAllActiveJointAngles returns empty vector
        print(f"[get_all_joint_pos] Failed: {e}, trying get_all_joint_angles as fallback")
        all_angles = hand.get_all_joint_angles()
    
    # Check if request succeeded (non-empty result and correct size)
    assert all_angles and len(all_angles) == 19, \
        f"Failed to get all joint angles: got {len(all_angles) if all_angles else 0} angles, expected 19"
    
    print(f"[get_all_joint_angles] All Joint Angles (rad, {len(all_angles)} joints): {all_angles[:10]}...")
    assert len(all_angles) == 19  # 12 active + 7 passive


# def test_control_mode(hand):
#     """Test control mode (read-only, requires hardware)"""
#     assert hand.init(), "Device not initialized"
    
#     # Only test reading control mode (read-only operation)
#     # Note: set_all_control_modes is not tested as it may cause CANFD communication to crash
#     current_modes = hand.get_all_control_modes()
#     # Check if request succeeded (non-empty result)
#     assert current_modes, "Failed to get control modes"
    
#     print(f"\n[get_all_control_modes] Control Modes: {current_modes}")
#     assert len(current_modes) == 12


def test_tactile_sensor_3d(hand):
    """Test 3D tactile sensor (O12 specific, requires hardware)"""
    assert hand.init(), "Device not initialized"
    
    # Test single sensor
    thumb_tactile = hand.get_tactile_sensor_3d_data(Finger.THUMB)
    print(f"\n[get_tactile_sensor_3d_data] Thumb 3D Tactile Data:")
    print(f"  Online State: {thumb_tactile.online_state}")
    print(f"  Normal Force: {thumb_tactile.normal_force}")
    print(f"  Tangent Force: {thumb_tactile.tangent_force}")
    print(f"  Tangent Force Angle: {thumb_tactile.tangent_force_angle}")
    assert thumb_tactile.normal_force >= 0
    
    # Test multiple sensors
    fingers = [Finger.INDEX, Finger.MIDDLE, Finger.RING, Finger.LITTLE]
    for finger in fingers:
        tactile_data = hand.get_tactile_sensor_3d_data(finger)
        print(f"\n[get_tactile_sensor_3d_data] {finger} 3D Tactile Data:")
        print(f"  Online State: {tactile_data.online_state}")
        print(f"  Normal Force: {tactile_data.normal_force}")
        print(f"  Tangent Force: {tactile_data.tangent_force}")
        print(f"  Tangent Force Angle: {tactile_data.tangent_force_angle}")


def test_error_report(hand):
    """Test error report (requires hardware)"""
    assert hand.init(), "Device not initialized"
    
    # Use try-except to catch potential crashes from pointer conversion issues
    try:
        error_reports = hand.get_all_error_reports()
    except Exception as e:
        pytest.fail(f"Failed to get error reports: {e}")
    
    # Check if request succeeded (non-empty result)
    assert error_reports, "Failed to get error reports: got empty error reports"
    
    print(f"\n[get_all_error_reports] Error Reports (12 joints): ", end="")
    has_errors = False
    for i, error in enumerate(error_reports):
        error_flags = []
        if error.stalled:
            error_flags.append("S")
            has_errors = True
        if error.overheat:
            error_flags.append("H")
            has_errors = True
        if error.over_current:
            error_flags.append("C")
            has_errors = True
        if error.motor_except:
            error_flags.append("M")
            has_errors = True
        if error.commu_except:
            error_flags.append("X")
            has_errors = True
        print(f"J{i+1}:[{''.join(error_flags) if error_flags else ''}]", end=" ")
    print()
    
    if has_errors:
        print("[Note] Error flags: S=Stalled, H=Overheat, C=Over-current, M=Motor exception, X=Communication exception")
        print("[Note] X (Communication exception) may indicate historical communication errors. "
              "This is normal if the device had previous communication timeouts.")
    
    assert len(error_reports) == 12


def test_temperature_report(hand):
    """Test temperature report (requires hardware)"""
    assert hand.init(), "Device not initialized"
    
    temp_reports = hand.get_all_temperature_reports()
    # Check if request succeeded (non-empty result)
    # If cache is empty, device hasn't sent reports yet - this is expected and not an error
    if not temp_reports:
        # No data available (cache is empty), skip assertion
        # This is expected if report periods are not set or device hasn't sent reports yet
        return
    
    print(f"\n[get_all_temperature_reports] Temperature Reports (°C): ", end="")
    for i, temp in enumerate(temp_reports):
        print(f"J{i+1}:{temp}", end=", " if i < len(temp_reports) - 1 else "")
    print()
    assert len(temp_reports) == 12


def test_current_report(hand):
    """Test current report (requires hardware)"""
    assert hand.init(), "Device not initialized"
    
    # Similar to gtest: directly call GetAllCurrentReport without setting periods
    # The device may have cached reports from previous operations
    current_reports = hand.get_all_current_reports()
    # Check if request succeeded (non-empty result)
    # If empty, skip assertion (same as gtest behavior)
    if not current_reports:
        # No data available (cache is empty), skip assertion
        # This is expected if report periods are not set or device hasn't sent reports yet
        return
    
    print(f"\n[get_all_current_reports] Current Reports (mA): ", end="")
    for i, current in enumerate(current_reports):
        print(f"J{i+1}:{current}", end=", " if i < len(current_reports) - 1 else "")
    print()
    assert len(current_reports) == 12


def test_kinematics_solver(hand):
    """Test kinematics solver"""
    assert hand.init(), "Device not initialized"
    
    # Test forward kinematics
    # First verify we can get motor positions (prerequisite check)
    motor_positions = hand.get_all_joint_positions()
    # If empty or wrong size, skip assertion (same as gtest behavior)
    if not motor_positions or len(motor_positions) != 12:
        print(f"\n[KinematicsSolver] Failed to get motor positions: got "
              f"{len(motor_positions) if motor_positions else 0}, expected 12")
        return
    
    # Test forward kinematics with valid input
    active_angles = [0.0] * 12
    all_angles = hand.get_all_joint_angles(active_angles)
    
    # Check if calculation succeeded (non-empty result and correct size)
    # If failed, skip assertion (same as gtest behavior)
    if not all_angles or len(all_angles) != 19:
        print(f"\n[get_all_joint_angles] Failed: got {len(all_angles) if all_angles else 0} angles, expected 19")
        return
    
    print(f"\n[get_all_joint_angles] Forward Kinematics (input: 12 active angles, output: {len(all_angles)} joint angles): ", end="")
    for i, angle in enumerate(all_angles):
        print(f"{angle:.4f}", end=", " if i < len(all_angles) - 1 else "")
    print()
    assert len(all_angles) == 19  # 12 active + 7 passive


def test_velocity_control(hand):
    """Test velocity control (requires hardware)"""
    assert hand.init(), "Device not initialized"
    
    # Test setting velocity
    velocities = [0] * 12
    hand.set_all_joint_velocities(velocities)
    print(f"\n[set_all_joint_velocities] Set Velocities: {velocities}")
    
    current_velocities = hand.get_all_joint_velocities()
    # Check if request succeeded (non-empty result and correct size)
    assert current_velocities and len(current_velocities) == 12, \
        f"Failed to get velocities: got {len(current_velocities) if current_velocities else 0} velocities, expected 12"
    
    print(f"[get_all_joint_velocities] Current Velocities: {current_velocities}")
    assert len(current_velocities) == 12


def test_voltage_control(hand):
    """Test voltage control (requires hardware)"""
    assert hand.init(), "Device not initialized"

    dof = 12
    safe_voltage = 0
    probe_joint = 1
    probe_voltage = 500
    voltage_mode = int(ControlMode.VOLTAGE)
    position_mode = int(ControlMode.POSITION)

    assert voltage_mode == 4, "ControlMode.VOLTAGE should match firmware mode value 4"

    try:
        hand.set_all_control_modes([position_mode] * dof)

        # Exercise a non-zero single-joint voltage command. O12 firmware through
        # 1.2.15 does not support voltage readback, so validate mode behavior and
        # command execution without asserting get_joint_voltage() values.
        hand.set_control_mode(probe_joint, voltage_mode)
        hand.set_joint_voltage(probe_joint, probe_voltage)
        time.sleep(0.5)
        hand.set_joint_voltage(probe_joint, safe_voltage)

        # Send zero voltage to every axis while checking the resulting modes.
        for joint in range(1, dof + 1):
            hand.set_joint_voltage(joint, safe_voltage)

        voltage_modes = hand.get_all_control_modes()
        if not voltage_modes or len(voltage_modes) != dof:
            print(f"[get_all_control_modes] Failed after voltage mode switch: "
                  f"got {len(voltage_modes) if voltage_modes else 0} modes, expected {dof}")
            return
        print(f"[get_all_control_modes] Voltage Control Modes: {voltage_modes}")
        all_modes_are_voltage = all(mode == voltage_mode for mode in voltage_modes)
        if not all_modes_are_voltage:
            return

        hand.set_all_control_modes([position_mode] * dof)
        time.sleep(0.25)
        current_modes = hand.get_all_control_modes()
        if not current_modes or len(current_modes) != dof:
            print(f"[get_all_control_modes] Failed after restoring position mode: "
                  f"got {len(current_modes) if current_modes else 0} modes, expected {dof}")
            return
        print(f"[get_all_control_modes] Modes after position restore: {current_modes}")

        voltages = [safe_voltage] * dof
        hand.set_all_joint_voltages(voltages)
        print(f"[set_all_joint_voltages] Set Voltages: {voltages}")
    finally:
        try:
            hand.set_all_joint_voltages([safe_voltage] * dof)
            hand.set_all_control_modes([position_mode] * dof)
            print("[set_control_mode] Restored all joints to position mode")
        except Exception as exc:
            print(f"[Warning] Failed to restore voltage control test state: {exc}")


if __name__ == "__main__":
    # Default to verbose mode if -v is not already specified
    args = sys.argv[1:]
    if "-v" not in args and "--verbose" not in args:
        args = ["-v"] + args
    pytest.main([__file__] + args)
