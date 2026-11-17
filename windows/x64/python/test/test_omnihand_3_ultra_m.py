# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Hardware pytest suite for OmniHand 3 Ultra M T1.3/T1.5.

This mirrors the safe public-API checks in
``cpp/test/test_omnihand_3_ultra_m.cc``.

Examples:

    python test_omnihand_3_ultra_m.py -d zlgcan -i 0 -c 1 --hand right
    python test_omnihand_3_ultra_m.py -d hcan --device-id 0
    python test_omnihand_3_ultra_m.py -d socketcan --can-if can0
    OMNIHAND_DEVICE_TYPE=zlgcan pytest -sv test_omnihand_3_ultra_m.py
"""

import os
import sys
import time

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
    HandType,
    H3UMGesture,
    H3UMRevision,
    OmniHand3UltraM,
    OmniHand3UltraMSolver,
)


TRANSPORTS = ("zlgcan", "hcan", "socketcan")
DEVICE_TYPE = "zlgcan"
REQUEST_INTERVAL = 5
CHANNEL_ID = 0
CANFD_ID = 0
CAN_IF = "can0"
FRAME_RECV_TIMEOUT = 200
HAND_TYPE_NAME = "left"

DEVICE_ID = 0  # Discover the address and detect T1.3/T1.5 on the selected channel.
PROFILE_POSITION_MODE = 7
POSITION_MODE = 0
MOTION_SETTLE_SECONDS = 2.0
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


@pytest.fixture(scope="session")
def hand():
    try:
        instance = _create_hand()
    except Exception as error:
        pytest.fail(f"Failed to create {DEVICE_TYPE} H3UM: {error}", pytrace=False)
    if instance is None:
        pytest.fail(f"{DEVICE_TYPE} factory returned no H3UM device", pytrace=False)

    instance.set_request_interval(REQUEST_INTERVAL)
    instance.set_frame_recv_timeout(FRAME_RECV_TIMEOUT)
    instance.show_data_details(True)
    print(
        f"\n[Info] H3UM revision={instance.get_revision()} "
        f"transport={DEVICE_TYPE} hand={HAND_TYPE_NAME} "
        f"requested_id={DEVICE_ID} interval={REQUEST_INTERVAL}ms "
        f"recv_timeout={FRAME_RECV_TIMEOUT}ms"
    )
    yield instance


@pytest.fixture(scope="session")
def initialized_hand(hand):
    if not hand.init():
        pytest.fail(
            f"{DEVICE_TYPE} H3UM is not connected or initialization failed",
            pytrace=False,
        )
    return hand


@pytest.fixture(scope="session")
def dof(initialized_hand):
    expected = {
        H3UMRevision.T1P3: 20,
        H3UMRevision.T1P5: 21,
    }[initialized_hand.get_revision()]
    assert initialized_hand.get_doa() == expected
    assert initialized_hand.get_num_of_joint_motors() == expected
    return expected


@pytest.fixture(scope="session")
def tactile_hand(initialized_hand):
    if not initialized_hand.init_tactile_points_map():
        pytest.skip("Tactile metadata unavailable")
    return initialized_hand


def _require_vector(values, expected_size, operation):
    values = list(values)
    if not values:
        pytest.skip(f"{operation} timed out or returned no data")
    assert len(values) == expected_size
    return values


def test_create_hand(hand):
    assert hand is not None


def test_init(initialized_hand):
    assert initialized_hand.init()


def test_vendor_info(initialized_hand, dof):
    info = initialized_hand.get_vendor_info()
    print(f"\n[GetVendorInfo]\n{info}")
    assert info.dof == dof
    assert len(initialized_hand.get_joint_names()) == dof


def test_device_info(initialized_hand):
    info = initialized_hand.get_device_info()
    print(f"\n[GetDeviceInfo]\n{info}")
    assert 1 <= info.hand_device_id <= 127
    if DEVICE_ID != 0:
        assert info.hand_device_id == DEVICE_ID


def test_gesture_dance_and_joint_4(initialized_hand, dof):
    pp_modes = [PROFILE_POSITION_MODE] * dof
    csp_modes = [POSITION_MODE] * dof

    initialized_hand.set_all_control_modes(pp_modes)
    time.sleep(0.1)
    try:
        for gesture in TEST_GESTURES:
            positions = initialized_hand.get_hand_gesture(gesture)
            assert len(positions) == dof
            feedback = initialized_hand.set_all_joint_positions(positions)
            print(f"\n[GestureDance] {gesture.name}: {positions}")
            assert len(feedback) == dof
            time.sleep(MOTION_SETTLE_SECONDS)

        # Motor 4 (pinky DIP) changes between these presets in both revisions.
        for gesture in TEST_GESTURES:
            position = initialized_hand.get_hand_gesture(gesture)[3]
            initialized_hand.set_joint_position(4, position)
            print(f"[GestureDance] Joint 4 -> {position}")
            time.sleep(MOTION_SETTLE_SECONDS)
            assert initialized_hand.get_joint_position(4) != -1
    finally:
        initialized_hand.set_all_control_modes(csp_modes)
        time.sleep(0.1)


def test_joint_positions(initialized_hand, dof):
    positions = _require_vector(
        initialized_hand.get_all_joint_positions(), dof, "GetAllJointMotorPosi"
    )
    # Feedback can exceed the command range; preserve the values from firmware.
    print(f"\n[GetAllJointMotorPosi] {positions}")


def test_active_joint_angles(initialized_hand, dof):
    solver = OmniHand3UltraMSolver(
        HAND_TYPE == HandType.LEFT, initialized_hand.get_revision()
    )
    for gesture in TEST_GESTURES:
        positions = initialized_hand.get_hand_gesture(gesture)
        assert len(positions) == dof
        angles = solver.actuator_input_to_active_joint_pos(positions)
        assert len(angles) == dof
        feedback = initialized_hand.set_all_active_joint_angles(angles)
        assert len(feedback) == dof
        time.sleep(1.0)
        actual = _require_vector(
            initialized_hand.get_all_active_joint_angles(), dof,
            "GetAllActiveJointAngles",
        )
        print(f"\n[{gesture.name}] target={angles}, actual={actual}")


def test_control_modes(initialized_hand, dof):
    modes = _require_vector(
        initialized_hand.get_all_control_modes(), dof, "GetAllControlMode"
    )
    print(f"\n[GetAllControlMode] {modes}")


def test_error_reports(initialized_hand, dof):
    reports = _require_vector(
        initialized_hand.get_all_error_reports(), dof, "GetAllErrorReport"
    )
    print(f"\n[GetAllErrorReport] {reports}")


def test_clear_error_reports(initialized_hand):
    initialized_hand.clear_all_error_report()
    initialized_hand.clear_error_report(1)


def test_temperature_reports(initialized_hand, dof):
    values = _require_vector(
        initialized_hand.get_all_temperature_reports(), dof, "GetAllTemperatureReport"
    )
    print(f"\n[GetAllTemperatureReport] {values}")


def test_current_reports(initialized_hand, dof):
    values = _require_vector(
        initialized_hand.get_all_current_reports(), dof, "GetAllCurrentReport"
    )
    print(f"\n[GetAllCurrentReport] {values}")


def test_joint_velocities(initialized_hand, dof):
    values = _require_vector(
        initialized_hand.get_all_joint_velocities(), dof, "GetAllJointMotorVelo"
    )
    print(f"\n[GetAllJointMotorVelo] {values}")


def test_all_tactile_sensor_data_raw(tactile_hand):
    # Whole-hand queries also support T1.5 firmware without single-region replies.
    sensors = _require_vector(
        tactile_hand.get_all_tactile_sensor_data_raw(),
        len(tactile_hand.get_sensor_order()), "GetAllTactileSensorDataRaw",
    )
    assert {sensor.sensor_id for sensor in sensors} == set(tactile_hand.get_sensor_order())
    for sensor in sensors:
        if tactile_hand.get_revision() == H3UMRevision.T1P5:
            values = sensor.data
        else:
            values = sensor.points
        assert len(values) == tactile_hand.get_num_of_tactile_points(sensor.sensor_id)
        assert values
        print(f"\n[Tactile] {sensor.sensor_id}: {len(values)} points")


def test_all_tactile_sensor_resultant_forces(tactile_hand):
    if tactile_hand.get_revision() != H3UMRevision.T1P3:
        pytest.skip("3D resultant forces require T1.3")
    forces = _require_vector(
        tactile_hand.get_all_tactile_sensor_resultant_forces(),
        len(tactile_hand.get_sensor_order()),
        "GetAllTactileSensorResultantForces",
    )
    for sensor in forces:
        force = sensor.force
        print(f"\n[ResultantForce] {sensor.sensor_id}: {force.x} {force.y} {force.z}")


if __name__ == "__main__":
    _export_config_to_environment()
    raise SystemExit(pytest.main([__file__, "-v", "-s", *sys.argv[1:]]))
