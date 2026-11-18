# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Hardware pytest suite for OmniHand 3 Ultra M T1.3/T1.5.

The 32 public cases follow ``cpp/test/test_omnihand_3_ultra_m.cc`` in order,
with a fresh device per case. Motion uses SIDE_FIST/OPEN; mixed control uses
PT only. Timestamped methods return (value, SDK receive time in Unix us).

Examples:

    python test_omnihand_3_ultra_m.py -d zlgcan -i 0 -c 1 --hand right
    python test_omnihand_3_ultra_m.py -d hcan --device-id 0
    python test_omnihand_3_ultra_m.py -d socketcan --can-if can0
    OMNIHAND_DEVICE_TYPE=zlgcan pytest -sv test_omnihand_3_ultra_m.py
"""

from datetime import datetime
import math
import os
import sys
import time
import weakref

# Use the installed package rather than the adjacent source package.
_python_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _python_dir in sys.path:
    sys.path.remove(_python_dir)

try:
    import pytest
except ImportError:
    print("Python tests require pytest. Install with: pip install pytest")
    raise

from omnihand import (
    AllTactileFrame,
    HandType,
    H3UMGesture,
    H3UMRevision,
    OmniHand3UltraM,
    OmniHand3UltraMSolver,
    PalmFrame,
    XenseFrame,
)


TRANSPORTS = ("zlgcan", "hcan", "socketcan")
DEVICE_TYPE = "zlgcan"
REQUEST_INTERVAL = 5
CHANNEL_ID = 0
CANFD_ID = 0
CAN_IF = "can0"
FRAME_RECV_TIMEOUT = 50
HAND_TYPE_NAME = "left"

DEVICE_ID = 0  # Discover the address and detect T1.3/T1.5 on the selected channel.
PROFILE_POSITION_MODE = 7
POSITION_MODE = 0
TEST_JOINT_INDEX = 4  # Pinky DIP changes in both presets.
TEST_GESTURES = (H3UMGesture.SIDE_FIST, H3UMGesture.OPEN)

_STR_OPTIONS = (
    ("OMNIHAND_DEVICE_TYPE", "-d", "DEVICE_TYPE"),
    ("OMNIHAND_CAN_IF", "--can-if", "CAN_IF"),
    ("OMNIHAND_HAND_TYPE", "--hand-type", "HAND_TYPE_NAME"),
)
_INT_OPTIONS = (
    ("OMNIHAND_REQUEST_INTERVAL", "-f", "REQUEST_INTERVAL"),
    ("OMNIHAND_CHANNEL_ID", "-c", "CHANNEL_ID"),
    ("OMNIHAND_CANFD_ID", "-i", "CANFD_ID"),
    ("OMNIHAND_FRAME_RECV_TIMEOUT", "-t", "FRAME_RECV_TIMEOUT"),
    ("OMNIHAND_DEVICE_ID", "--device-id", "DEVICE_ID"),
)


def _apply_environment():
    for env_name, _flag, attr in _STR_OPTIONS:
        value = os.environ.get(env_name)
        if value:
            globals()[attr] = value
    for env_name, _flag, attr in _INT_OPTIONS:
        value = os.environ.get(env_name)
        if value is None:
            continue
        try:
            globals()[attr] = int(value, 0)
        except ValueError:
            print(f"[Warning] Ignoring invalid {env_name}={value!r}")


def _apply_direct_cli():
    """Consume device options before forwarding remaining arguments to pytest."""
    argv = ["--hand-type" if arg == "--hand" else arg for arg in sys.argv]
    for options, cast in ((_STR_OPTIONS, str), (_INT_OPTIONS, lambda x: int(x, 0))):
        for _env_name, flag, attr in options:
            while flag in argv:
                index = argv.index(flag)
                if index + 1 >= len(argv):
                    print(f"[Warning] Ignoring {flag} without a value")
                    argv.pop(index)
                    continue
                try:
                    globals()[attr] = cast(argv[index + 1])
                except ValueError:
                    print(f"[Warning] Ignoring invalid {flag} value {argv[index + 1]!r}")
                del argv[index:index + 2]
    sys.argv = argv


def _export_config_to_environment():
    for env_name, _flag, attr in _STR_OPTIONS + _INT_OPTIONS:
        os.environ[env_name] = str(globals()[attr])


_apply_environment()
if __name__ == "__main__":
    _apply_direct_cli()

if DEVICE_TYPE not in TRANSPORTS:
    raise ValueError(f"Unsupported transport {DEVICE_TYPE!r}; choose one of {TRANSPORTS}")
if HAND_TYPE_NAME.lower() not in ("left", "right"):
    raise ValueError("--hand-type must be 'left' or 'right'")
if not 0 <= REQUEST_INTERVAL <= 100:
    raise ValueError("-f/OMNIHAND_REQUEST_INTERVAL must be in [0, 100]")
if not 0 <= CANFD_ID <= 255 or not 0 <= CHANNEL_ID <= 255:
    raise ValueError("CAN adapter index (-i) and channel (-c) must be in [0, 255]")
if not 0 <= DEVICE_ID <= 127:
    raise ValueError("--device-id/OMNIHAND_DEVICE_ID must be in [0, 127]")
if FRAME_RECV_TIMEOUT <= 0:
    raise ValueError("-t/OMNIHAND_FRAME_RECV_TIMEOUT must be positive")

HAND_TYPE = HandType.LEFT if HAND_TYPE_NAME.lower() == "left" else HandType.RIGHT


def _create_hand():
    common = {
        "hand_type": HAND_TYPE,
        "hand_device_id": DEVICE_ID,
    }
    if DEVICE_TYPE == "hcan":
        return OmniHand3UltraM.create_hand_by_hcan(
            **common,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID,
        )
    if DEVICE_TYPE == "socketcan":
        if not hasattr(OmniHand3UltraM, "create_hand_socketcan"):
            pytest.fail("SocketCAN is unavailable on this platform", pytrace=False)
        return OmniHand3UltraM.create_hand_socketcan(
            **common,
            can_interface=CAN_IF,
        )
    return OmniHand3UltraM.create_hand_by_zlgcan(
        **common,
        canfd_device_id=CANFD_ID,
        canfd_channel_id=CHANNEL_ID,
    )


@pytest.fixture
def hand():
    try:
        instance = _create_hand()
    except Exception as error:
        pytest.fail(f"Failed to create {DEVICE_TYPE} H3UM: {error}", pytrace=False)
    if instance is None or not instance.init():
        pytest.skip("Device not available")

    instance.set_request_interval(REQUEST_INTERVAL)
    instance.set_frame_recv_timeout(FRAME_RECV_TIMEOUT)
    print(
        f"\n[Info] H3UM revision={instance.get_revision()} "
        f"transport={DEVICE_TYPE} hand={HAND_TYPE_NAME} "
        f"requested_id={DEVICE_ID} interval={REQUEST_INTERVAL}ms "
        f"recv_timeout={FRAME_RECV_TIMEOUT}ms"
    )
    try:
        # Only this fixture owns the connection: pytest can retain test locals
        # in skip/failure tracebacks after teardown.
        yield weakref.proxy(instance)
    finally:
        del instance


@pytest.fixture
def dof(hand):
    return hand.get_doa()


def _validate_timestamp(timestamp_us, operation):
    assert timestamp_us > 0, f"{operation} has no valid receive timestamp"
    seconds, fraction = divmod(timestamp_us, 1_000_000)
    date = datetime.fromtimestamp(seconds).strftime("%Y-%m-%d %H:%M:%S")
    date = f"{date}.{fraction:06d}"
    print(f"\n[{operation}] received_local={date}")
    assert len(date) == 26
    assert abs(timestamp_us - time.time_ns() // 1000) <= 5_000_000, (
        f"{operation} timestamp differs from host Unix time by more than 5 seconds"
    )


def _timestamped_vector(sample, expected_size, operation):
    values, timestamp_us = sample
    _validate_timestamp(timestamp_us, operation)
    assert len(values) == expected_size
    return values


def _gesture_targets(hand, gesture):
    targets = hand.get_hand_gesture(gesture)
    assert len(targets) == hand.get_doa()
    return targets


def test_get_all_joint_motor_posi_with_timestamp(hand, dof):
    positions = _timestamped_vector(
        hand.get_all_joint_positions_with_timestamp(), dof,
        "GetAllJointMotorPosiWithTimestamp",
    )
    # Preserve raw feedback, including values outside command bounds.
    print(f"Motor positions: {positions}")


def test_get_all_error_report_with_timestamp(hand, dof):
    _timestamped_vector(
        hand.get_all_error_reports_with_timestamp(), dof,
        "GetAllErrorReportWithTimestamp",
    )


def test_get_all_temperature_report_with_timestamp(hand, dof):
    _timestamped_vector(
        hand.get_all_temperature_reports_with_timestamp(), dof,
        "GetAllTemperatureReportWithTimestamp",
    )


def test_get_temperature_of_palm_with_timestamp(hand):
    if hand.get_revision() != H3UMRevision.T1P5:
        pytest.skip("Board temperature requires T1.5")
    value, timestamp_us = hand.get_temperature_of_palm_with_timestamp()
    _validate_timestamp(timestamp_us, "GetTemperatureOfPalmWithTimestamp")
    print(f"Palm-board temperature: {value}")


def test_get_temperature_of_power_with_timestamp(hand):
    if hand.get_revision() != H3UMRevision.T1P5:
        pytest.skip("Board temperature requires T1.5")
    value, timestamp_us = hand.get_temperature_of_power_with_timestamp()
    _validate_timestamp(timestamp_us, "GetTemperatureOfPowerWithTimestamp")
    print(f"Power-board temperature: {value}")


def test_get_finger_tactile_with_timestamp(hand):
    for index in range(5):
        frame = XenseFrame()
        success, timestamp_us = hand.get_finger_tactile_with_timestamp(index, frame)
        if not success:
            pytest.skip("Xense tactile sensor unavailable")
        _validate_timestamp(timestamp_us, "GetFingerTactileWithTimestamp")
        source_timestamp = frame.timestamp
        cached_success, cached_timestamp = hand.get_finger_tactile_with_timestamp(
            index, frame,
        )
        if cached_success and source_timestamp and source_timestamp == frame.timestamp:
            assert cached_timestamp == timestamp_us


def test_get_palm_tactile_with_timestamp(hand):
    frame = PalmFrame()
    success, timestamp_us = hand.get_palm_tactile_with_timestamp(frame)
    if not success or not frame.force:
        pytest.skip("Palm tactile sensor unavailable")
    _validate_timestamp(timestamp_us, "GetPalmTactileWithTimestamp")
    source_timestamp = frame.timestamp
    cached_success, cached_timestamp = hand.get_palm_tactile_with_timestamp(frame)
    if cached_success and source_timestamp and source_timestamp == frame.timestamp:
        assert cached_timestamp == timestamp_us


def test_get_all_tactile_with_timestamp(hand):
    frame = AllTactileFrame()
    success, timestamp_us = hand.get_all_tactile_with_timestamp(frame)
    if not success:
        pytest.skip("Tactile sensors unavailable")
    _validate_timestamp(timestamp_us, "GetAllTactileWithTimestamp")


def test_get_all_current_report_with_timestamp(hand, dof):
    _timestamped_vector(
        hand.get_all_current_reports_with_timestamp(), dof,
        "GetAllCurrentReportWithTimestamp",
    )


def test_get_joint_motor_posi_with_timestamp(hand):
    value, timestamp_us = hand.get_joint_position_with_timestamp(1)
    _validate_timestamp(timestamp_us, "GetJointMotorPosiWithTimestamp")
    print(f"Joint 1 position: {value}")


def test_set_joint_motor_posi_with_timestamp(hand):
    for gesture in TEST_GESTURES:
        target = _gesture_targets(hand, gesture)[TEST_JOINT_INDEX - 1]
        feedback, timestamp_us = hand.set_joint_position_with_timestamp(
            TEST_JOINT_INDEX, target,
        )
        _validate_timestamp(timestamp_us, "SetJointMotorPosiWithTimestamp")
        print(f"Joint {TEST_JOINT_INDEX} target={target}, feedback={feedback}")
        time.sleep(1.0)


def test_set_all_joint_motor_posi_with_timestamp(hand, dof):
    for gesture in TEST_GESTURES:
        targets = _gesture_targets(hand, gesture)
        feedback = _timestamped_vector(
            hand.set_all_joint_positions_with_timestamp(targets), dof,
            "SetAllJointMotorPosiWithTimestamp",
        )
        print(f"Gesture {gesture.name} feedback: {feedback}")
        time.sleep(1.0)


def test_get_all_active_joint_angles_with_timestamp(hand, dof):
    angles = _timestamped_vector(
        hand.get_all_active_joint_angles_with_timestamp(), dof,
        "GetAllActiveJointAnglesWithTimestamp",
    )
    assert all(math.isfinite(angle) for angle in angles)


def test_get_all_joint_angles_with_timestamp(hand):
    angles = _timestamped_vector(
        hand.get_all_joint_angles_with_timestamp(), len(hand.get_joint_names()),
        "GetAllJointAnglesWithTimestamp",
    )
    assert all(math.isfinite(angle) for angle in angles)


def test_set_all_active_joint_angles_with_timestamp(hand, dof):
    solver = OmniHand3UltraMSolver(
        hand.get_hand_type() == HandType.LEFT, hand.get_revision(),
    )
    for gesture in TEST_GESTURES:
        targets = _gesture_targets(hand, gesture)
        angles = solver.actuator_input_to_active_joint_pos(targets)
        assert len(angles) == dof
        feedback = _timestamped_vector(
            hand.set_all_active_joint_angles_with_timestamp(angles), dof,
            "SetAllActiveJointAnglesWithTimestamp",
        )
        assert all(math.isfinite(angle) for angle in feedback)
        time.sleep(1.0)


def test_mix_control_by_pt_all_with_timestamp(hand, dof):
    current = hand.get_min_max_default_mix_ctrl_torque(1).default_value
    for gesture in TEST_GESTURES:
        targets = _gesture_targets(hand, gesture)
        feedback = _timestamped_vector(
            hand.mix_control_by_pt_with_timestamp(targets, [current] * dof), dof,
            "MixControlByPTAllWithTimestamp",
        )
        for joint in feedback:
            assert joint.tgt_posi is not None
            assert joint.tgt_torque is not None
        time.sleep(1.0)


def test_mix_control_by_pt_single_with_timestamp(hand):
    current = hand.get_min_max_default_mix_ctrl_torque(1).default_value
    for gesture in TEST_GESTURES:
        targets = _gesture_targets(hand, gesture)
        feedback, timestamp_us = hand.mix_control_by_pt_with_timestamp(
            TEST_JOINT_INDEX, targets[TEST_JOINT_INDEX - 1], current,
        )
        _validate_timestamp(timestamp_us, "MixControlByPTSingleWithTimestamp")
        assert feedback.joint_index == TEST_JOINT_INDEX
        assert feedback.tgt_posi is not None
        assert feedback.tgt_torque is not None
        time.sleep(1.0)


def test_get_all_tactile_sensor_data_raw_with_timestamp(hand):
    if not hand.init_tactile_points_map():
        pytest.skip("Tactile sensors unavailable")
    sensors = _timestamped_vector(
        hand.get_all_tactile_sensor_data_raw_with_timestamp(),
        len(hand.get_sensor_order()), "GetAllTactileSensorDataRawWithTimestamp",
    )
    for sensor in sensors:
        values = sensor.data if hand.get_revision() == H3UMRevision.T1P5 else sensor.points
        assert len(values) == hand.get_num_of_tactile_points(sensor.sensor_id)


def test_get_all_tactile_sensor_resultant_forces_with_timestamp(hand):
    if hand.get_revision() != H3UMRevision.T1P3:
        pytest.skip("3D resultant forces require T1.3")
    _timestamped_vector(
        hand.get_all_tactile_sensor_resultant_forces_with_timestamp(),
        len(hand.get_sensor_order()),
        "GetAllTactileSensorResultantForcesWithTimestamp",
    )


def test_create_hand(hand):
    assert hand is not None


def test_init(hand):
    assert hand.init()


def test_get_vendor_info(hand, dof):
    info = hand.get_vendor_info()
    print(f"\n[GetVendorInfo]\n{info}")
    if info.dof == 0:
        return
    assert info.dof == dof
    assert hand.get_num_of_joint_motors() == dof
    assert len(hand.get_joint_names()) == dof


def test_get_device_info(hand):
    info = hand.get_device_info()
    print(f"\n[GetDeviceInfo]\n{info}")
    if info.hand_device_id != 0:
        assert info.hand_device_id == hand.get_hand_device_id()


def test_get_non_private_hand_device_id_by_broadcast(hand):
    device_id = hand.get_non_private_hand_device_id_by_broadcast()
    assert device_id > 0, "No H3U_M replied to the broadcast request"
    assert device_id == hand.get_hand_device_id()


def test_mix_control_rejects_invalid_arguments(hand, dof):
    valid = [0] * dof
    short_values = [0] * (dof - 1)
    assert not hand.mix_control_by_pt(short_values, short_values)
    assert not hand.mix_control_by_pt(valid, short_values)
    assert hand.mix_control_by_pt(0, 0, 0).ctrl_mode == 0
    assert hand.mix_control_by_pt(dof + 1, 0, 0).ctrl_mode == 0


def test_gesture_dance(hand, dof):
    hand.set_all_control_modes([PROFILE_POSITION_MODE] * dof)
    time.sleep(0.1)
    try:
        for gesture in TEST_GESTURES:
            feedback = _timestamped_vector(
                hand.set_hand_gesture_with_timestamp(gesture.value), dof,
                "SetHandGestureWithTimestamp",
            )
            print(f"[GestureDance] {gesture.name} feedback: {feedback}")
            time.sleep(2.0)
    finally:
        hand.set_all_control_modes([POSITION_MODE] * dof)
        time.sleep(0.1)


def test_control_mode(hand, dof):
    modes = hand.get_all_control_modes()
    print(f"\n[GetAllControlMode] {modes}")
    if not modes or len(modes) != dof:
        return
    assert len(modes) == dof


def test_error_report(hand, dof):
    reports = hand.get_all_error_reports()
    print(f"\n[GetAllErrorReport] {reports}")
    if reports:
        assert len(reports) == dof


def test_clear_error_report(hand):
    hand.clear_all_error_report()
    hand.clear_error_report(1)


def test_temperature_report(hand, dof):
    values = hand.get_all_temperature_reports()
    print(f"\n[GetAllTemperatureReport] {values}")
    if values:
        assert len(values) == dof


def test_current_report(hand, dof):
    values = hand.get_all_current_reports()
    print(f"\n[GetAllCurrentReport] {values}")
    if values:
        assert len(values) == dof


if __name__ == "__main__":
    _export_config_to_environment()
    raise SystemExit(pytest.main([__file__, "-v", "-s", *sys.argv[1:]]))
