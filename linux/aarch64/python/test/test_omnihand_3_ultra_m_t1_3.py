# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Hardware pytest suite for OmniHand 3 Ultra M T1.3.

This mirrors the safe public-API checks in
``cpp/test/test_omnihand_3_ultra_m_t1.3.cc``.

Examples:

    python test_omnihand_3_ultra_m_t1_3.py -d hcan -i 0 -c 0
    python test_omnihand_3_ultra_m_t1_3.py -d socketcan --can-if can0
    OMNIHAND_DEVICE_TYPE=zlgcan pytest -sv test_omnihand_3_ultra_m_t1_3.py
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

from omnihand import HandType, H3UMRevision, OmniHand3UltraM


TRANSPORTS = ("zlgcan", "hcan", "socketcan")
DEVICE_TYPE = "zlgcan"
REQUEST_INTERVAL = 5
CHANNEL_ID = 0
CANFD_ID = 0
CAN_IF = "can0"
FRAME_RECV_TIMEOUT = 200
HAND_TYPE_NAME = "left"

DOF = 20
DEVICE_ID = 9
PROFILE_POSITION_MODE = 7
POSITION_MODE = 0
MOTION_SETTLE_SECONDS = 2.0

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
    argv = sys.argv[:]
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
        pytest.fail(f"Failed to create {DEVICE_TYPE} H3UM T1.3: {error}", pytrace=False)
    if instance is None:
        pytest.fail(f"{DEVICE_TYPE} factory returned no H3UM T1.3 device", pytrace=False)

    instance.set_request_interval(REQUEST_INTERVAL)
    instance.set_frame_recv_timeout(FRAME_RECV_TIMEOUT)
    instance.show_data_details(True)
    print(
        f"\n[Info] H3UM T1.3 transport={DEVICE_TYPE} hand={HAND_TYPE_NAME} "
        f"device_id={DEVICE_ID} interval={REQUEST_INTERVAL}ms "
        f"recv_timeout={FRAME_RECV_TIMEOUT}ms"
    )
    yield instance


@pytest.fixture(scope="session")
def initialized_hand(hand):
    if not hand.init():
        pytest.fail(
            f"{DEVICE_TYPE} H3UM T1.3 is not connected or initialization failed",
            pytrace=False,
        )
    return hand


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


def test_vendor_info(initialized_hand):
    info = initialized_hand.get_vendor_info()
    print(f"\n[GetVendorInfo]\n{info}")
    assert info.dof == DOF
    assert initialized_hand.get_doa() == DOF
    assert initialized_hand.get_num_of_joint_motors() == DOF
    assert initialized_hand.get_revision() == H3UMRevision.T1P3
    assert len(initialized_hand.get_joint_names()) == DOF


def test_device_info(initialized_hand):
    info = initialized_hand.get_device_info()
    print(f"\n[GetDeviceInfo]\n{info}")
    if info.hand_device_id != 0:
        assert info.hand_device_id == DEVICE_ID


def test_gesture_dance_and_joint_4(initialized_hand):
    pp_modes = [PROFILE_POSITION_MODE] * DOF
    csp_modes = [POSITION_MODE] * DOF
    gestures = (
        [2048, 512, 512, 512, 2048, 512, 512, 512, 2048, 512,
         512, 512, 2048, 512, 512, 512, 3192, 1024, 512, 512],
        [2048, 4096, 4096, 2048, 2048, 4096, 4096, 2048, 2048, 4096,
         4096, 2048, 2048, 4096, 512, 512, 3156, 1349, 512, 512],
    )

    initialized_hand.set_all_control_modes(pp_modes)
    time.sleep(0.1)
    try:
        for index, positions in enumerate(gestures, start=1):
            feedback = initialized_hand.set_all_joint_positions(positions)
            print(f"\n[GestureDance] Gesture {index}/{len(gestures)}: {positions}")
            assert len(feedback) == DOF
            time.sleep(MOTION_SETTLE_SECONDS)

        for position in (512, 4096):
            initialized_hand.set_joint_position(4, position)
            print(f"[GestureDance] Joint 4 -> {position}")
            time.sleep(MOTION_SETTLE_SECONDS)
    finally:
        initialized_hand.set_all_control_modes(csp_modes)
        time.sleep(0.1)


def test_control_modes(initialized_hand):
    modes = _require_vector(
        initialized_hand.get_all_control_modes(), DOF, "GetAllControlMode"
    )
    print(f"\n[GetAllControlMode] {modes}")


def test_error_reports(initialized_hand):
    reports = _require_vector(
        initialized_hand.get_all_error_reports(), DOF, "GetAllErrorReport"
    )
    print(f"\n[GetAllErrorReport] {reports}")


def test_clear_error_reports(initialized_hand):
    initialized_hand.clear_all_error_report()
    initialized_hand.clear_error_report(1)


def test_temperature_reports(initialized_hand):
    values = _require_vector(
        initialized_hand.get_all_temperature_reports(), DOF, "GetAllTemperatureReport"
    )
    print(f"\n[GetAllTemperatureReport] {values}")


def test_current_reports(initialized_hand):
    values = _require_vector(
        initialized_hand.get_all_current_reports(), DOF, "GetAllCurrentReport"
    )
    print(f"\n[GetAllCurrentReport] {values}")


def test_joint_velocities(initialized_hand):
    values = _require_vector(
        initialized_hand.get_all_joint_velocities(), DOF, "GetAllJointMotorVelo"
    )
    print(f"\n[GetAllJointMotorVelo] {values}")


if __name__ == "__main__":
    _export_config_to_environment()
    raise SystemExit(pytest.main([__file__, "-v", "-s", *sys.argv[1:]]))
