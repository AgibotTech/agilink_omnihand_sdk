# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Unified hardware tests for OmniHand 3 Lite (H3L, 4 DOF).

The test cases mirror the safe public-API coverage in
``cpp/test/test_omnihand_3_lite.cc``. Calibration, homing, parameter
persistence, firmware maintenance, and other destructive operations are
deliberately excluded. Device-ID changes run only when ``--device-id`` is
explicitly supplied.

Transport is selected with ``-d`` when this file is run directly, or with
``OMNIHAND_DEVICE_TYPE`` when it is run through pytest:

    zlgcan | hcan | socketcan | zlgcantcp | rs485

Examples:

    python test_omnihand_3_lite.py -d hcan -i 0 -c 0
    python test_omnihand_3_lite.py -d rs485 -p COM7
    OMNIHAND_DEVICE_TYPE=hcan pytest -sv test_omnihand_3_lite.py

Environment variables:

    OMNIHAND_DEVICE_TYPE, OMNIHAND_REQUEST_INTERVAL,
    OMNIHAND_CHANNEL_ID, OMNIHAND_CANFD_ID, OMNIHAND_CAN_IF,
    OMNIHAND_TCP_HOST, OMNIHAND_TCP_PORT, OMNIHAND_RS485_PORT,
    OMNIHAND_BAUDRATE, OMNIHAND_FRAME_RECV_TIMEOUT,
    OMNIHAND_TARGET_DEVICE_ID, OMNIHAND_HAND_TYPE, OMNIHAND_H3L_MODEL

Each hardware case creates a fresh device, using the gtest defaults of a
5 ms request interval and a 100 ms receive timeout.
"""

import os
import platform
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
    Finger,
    HandType,
    OmniHand3Lite,
    OmniHand3LiteGesture,
)


CAN_TRANSPORTS = ("zlgcan", "hcan", "socketcan", "zlgcantcp")
TRANSPORTS = CAN_TRANSPORTS + ("rs485",)

DEVICE_TYPE = "zlgcan"
REQUEST_INTERVAL = 5
CHANNEL_ID = 0
CANFD_ID = 0
CAN_IF = "can0"
TCP_HOST = "192.168.0.178"
TCP_PORT = 8000
RS485_PORT = "COM5" if platform.system() == "Windows" else "/dev/ttyUSB0"
BAUDRATE = 460800
FRAME_RECV_TIMEOUT = 100
TARGET_DEVICE_ID = None
HAND_TYPE_NAME = "left"
MODEL_NAME = "s"

_STR_OPTIONS = (
    ("OMNIHAND_DEVICE_TYPE", "-d", "DEVICE_TYPE"),
    ("OMNIHAND_DEVICE_TYPE", "--device", "DEVICE_TYPE"),
    ("OMNIHAND_CAN_IF", "--can-if", "CAN_IF"),
    ("OMNIHAND_TCP_HOST", "--tcp-host", "TCP_HOST"),
    ("OMNIHAND_RS485_PORT", "-p", "RS485_PORT"),
    ("OMNIHAND_RS485_PORT", "--port", "RS485_PORT"),
    ("OMNIHAND_RS485_PORT", "-P", "RS485_PORT"),
    ("OMNIHAND_HAND_TYPE", "--hand-type", "HAND_TYPE_NAME"),
    ("OMNIHAND_H3L_MODEL", "--model", "MODEL_NAME"),
)
_INT_OPTIONS = (
    ("OMNIHAND_REQUEST_INTERVAL", "-f", "REQUEST_INTERVAL"),
    ("OMNIHAND_REQUEST_INTERVAL", "--request-interval", "REQUEST_INTERVAL"),
    ("OMNIHAND_CHANNEL_ID", "-c", "CHANNEL_ID"),
    ("OMNIHAND_CANFD_ID", "-i", "CANFD_ID"),
    ("OMNIHAND_TCP_PORT", "--tcp-port", "TCP_PORT"),
    ("OMNIHAND_BAUDRATE", "-b", "BAUDRATE"),
    ("OMNIHAND_FRAME_RECV_TIMEOUT", "-t", "FRAME_RECV_TIMEOUT"),
    ("OMNIHAND_TARGET_DEVICE_ID", "--device-id", "TARGET_DEVICE_ID"),
    ("OMNIHAND_TARGET_DEVICE_ID", "--id", "TARGET_DEVICE_ID"),
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
    """Consume product-specific arguments before passing the rest to pytest."""
    argv = sys.argv[:]
    for options, cast in ((_STR_OPTIONS, str), (_INT_OPTIONS, lambda x: int(x, 0))):
        for _env_name, flag, attr in options:
            while flag in argv:
                index = argv.index(flag)
                if index + 1 >= len(argv):
                    print(f"[Warning] Ignoring {flag} without a value")
                    argv.pop(index)
                    continue
                value = argv[index + 1]
                try:
                    globals()[attr] = cast(value)
                except ValueError:
                    print(f"[Warning] Ignoring invalid {flag} value {value!r}")
                del argv[index:index + 2]
    sys.argv = argv


def _export_config_to_environment():
    """Preserve direct-run options when pytest imports this file again."""
    for env_name, _flag, attr in _STR_OPTIONS + _INT_OPTIONS:
        value = globals()[attr]
        if value is None:
            os.environ.pop(env_name, None)
        else:
            os.environ[env_name] = str(value)


_apply_environment()
if __name__ == "__main__":
    _apply_direct_cli()

if DEVICE_TYPE not in TRANSPORTS:
    print(f"[Warning] Unknown transport {DEVICE_TYPE!r}; using zlgcan")
    DEVICE_TYPE = "zlgcan"
if HAND_TYPE_NAME.lower() not in ("left", "right"):
    print(f"[Warning] Unknown hand type {HAND_TYPE_NAME!r}; using left")
    HAND_TYPE_NAME = "left"
if TARGET_DEVICE_ID is not None and not 1 <= TARGET_DEVICE_ID <= 0x7F:
    raise ValueError("--device-id must be in [1, 0x7f]")
if MODEL_NAME not in ("s", "m"):
    raise ValueError("--model must be s or m")
REQUEST_INTERVAL = max(0, min(100, REQUEST_INTERVAL))
HAND_TYPE = HandType.LEFT if HAND_TYPE_NAME.lower() == "left" else HandType.RIGHT

DOF = 4

# Match the explicit 500 ms waits in gtest position and angle controls.
MOTION_SETTLE_SECONDS = 0.5


def _create_hand():
    factory_prefix = "create_hand_m" if MODEL_NAME == "m" else "create_hand"
    common = {
        "hand_type": HAND_TYPE,
        "hand_device_id": 0,
    }
    if DEVICE_TYPE == "hcan":
        return getattr(OmniHand3Lite, factory_prefix + "_by_hcan")(
            **common,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID,
        )
    if DEVICE_TYPE == "socketcan":
        return getattr(OmniHand3Lite, factory_prefix + "_socketcan")(
            **common,
            can_interface=CAN_IF,
        )
    if DEVICE_TYPE == "zlgcantcp":
        return getattr(OmniHand3Lite, factory_prefix + "_by_zlgcan_tcp")(
            **common,
            host=TCP_HOST,
            port=TCP_PORT,
            canfd_channel_id=CHANNEL_ID,
        )
    if DEVICE_TYPE == "rs485":
        return getattr(OmniHand3Lite, factory_prefix + "_by_rs485")(
            **common,
            serial_port=RS485_PORT,
            baud_rate=BAUDRATE,
        )
    return getattr(OmniHand3Lite, factory_prefix + "_by_zlgcan")(
        **common,
        canfd_device_id=CANFD_ID,
        canfd_channel_id=CHANNEL_ID,
    )


@pytest.fixture
def hand():
    """Create and initialize a fresh device for each hardware case."""
    try:
        instance = _create_hand()
    except Exception as error:
        pytest.fail(
            f"Failed to create {DEVICE_TYPE} H3L device: {error}",
            pytrace=False,
        )
    if instance is None or not instance.init():
        pytest.skip("Failed to initialize device")

    instance.set_request_interval(REQUEST_INTERVAL)
    instance.set_frame_recv_timeout(FRAME_RECV_TIMEOUT)
    instance.show_data_details(True)
    print(
        f"\n[Info] transport={DEVICE_TYPE} model={MODEL_NAME} hand={HAND_TYPE_NAME} "
        f"target_device_id={TARGET_DEVICE_ID} interval={REQUEST_INTERVAL}ms "
        f"recv_timeout={FRAME_RECV_TIMEOUT}ms"
    )
    if DEVICE_TYPE in ("zlgcan", "hcan", "zlgcantcp"):
        print(f"[Info] canfd_id={CANFD_ID} channel={CHANNEL_ID}")
    elif DEVICE_TYPE == "socketcan":
        print(f"[Info] can_interface={CAN_IF}")
    else:
        print(f"[Info] port={RS485_PORT} baudrate={BAUDRATE}")
    yield instance


@pytest.fixture
def original_device_id(hand):
    device_id = hand.get_hand_device_id_by_broadcast()
    if device_id < 0:
        pytest.skip("No consistent device ID responded to broadcast")
    print(f"\n[Info] Discovered device ID state: {device_id}")
    return device_id


@pytest.fixture
def initialized_hand(hand, original_device_id):
    id_change_attempted = False
    try:
        if TARGET_DEVICE_ID is not None:
            print(
                f"[Info] Switching device ID state: "
                f"{original_device_id} -> {TARGET_DEVICE_ID}"
            )
            id_change_attempted = True
            assert hand.set_hand_device_id_by_broadcast(TARGET_DEVICE_ID)
        yield hand
    finally:
        if hand.init() and id_change_attempted:
            print(
                f"\n[Info] Restoring device ID state: "
                f"{TARGET_DEVICE_ID} -> {original_device_id}"
            )
            assert hand.set_hand_device_id_by_broadcast(original_device_id)


@pytest.fixture
def tactile_sensors(initialized_hand):
    assert initialized_hand.init()
    if not initialized_hand.init_tactile_points_map():
        pytest.skip("Tactile sensors not available")
    sensors = list(initialized_hand.get_sensor_order())
    if not sensors:
        pytest.fail("H3L returned an empty tactile sensor order", pytrace=False)
    return sensors


# Connection and product information

def test_create_hand(initialized_hand):
    assert initialized_hand is not None


def test_init(initialized_hand):
    assert initialized_hand.init()


def test_vendor_info(initialized_hand):
    assert initialized_hand.init()
    info = initialized_hand.get_vendor_info()
    print(f"\n[GetVendorInfo]\n{info}")
    assert info.dof == DOF


def test_device_info(initialized_hand, original_device_id):
    assert initialized_hand.init()
    info = initialized_hand.get_device_info()
    print(f"\n[GetDeviceInfo]\n{info}")
    expected_id = (
        TARGET_DEVICE_ID if TARGET_DEVICE_ID is not None else
        OmniHand3Lite.kDefaultHandDeviceId if original_device_id == 0 else original_device_id
    )
    assert info.hand_device_id == expected_id


# Position and angle control

def test_motor_position_control(initialized_hand):
    assert initialized_hand.init()

    # Test single motor position
    target_pos = 4095
    initialized_hand.set_joint_position(1, target_pos)
    print(f"\n[SetJointMotorPosi] Set Joint 1 Motor Position: {target_pos}")
    time.sleep(MOTION_SETTLE_SECONDS)

    pos = initialized_hand.get_joint_position(1)
    print(f"[GetJointMotorPosi] Joint 1 Motor Position: {pos}")

    # Test batch motor positions
    positions = [3072] * DOF  # 4 motors, all at middle
    set_result = initialized_hand.set_all_joint_positions(positions)
    print(f"[SetAllJointMotorPosi] returned: {set_result}")
    time.sleep(MOTION_SETTLE_SECONDS)

    # Test GetAllJointMotorPosi separately
    all_positions = initialized_hand.get_all_joint_positions()
    print(f"[GetAllJointMotorPosi] returned: {all_positions}")
    if not all_positions:
        return
    print(f"[GetAllJointMotorPosi] Motor Positions: {all_positions}")
    assert len(all_positions) == DOF


def test_joint_angle_control(initialized_hand):
    assert initialized_hand.init()

    # Test setting active joint angles
    angles = [0.0] * DOF  # 4 joints, all at 0
    actual = initialized_hand.set_all_active_joint_angles(angles)
    print(f"\n[SetAllActiveJointAngles] Called (should output warning)")
    print(f"[SetAllActiveJointAngles] returned: {actual}")
    time.sleep(MOTION_SETTLE_SECONDS)

    # Test getting active joint angles
    active = initialized_hand.get_all_active_joint_angles()
    all_angles = initialized_hand.get_all_joint_angles()
    print(f"[GetAllActiveJointAngles] {active}")
    print(f"[GetAllJointAngles] {all_angles}")
    assert len(active) == DOF
    assert len(all_angles) == DOF


def test_hand_gesture(initialized_hand):
    assert initialized_hand.init()

    gesture = OmniHand3LiteGesture.OMNI_HAND_3_LITE_GESTURE_OK
    initialized_hand.set_hand_gesture(gesture)
    print(f"\n[SetHandGesture] OK gesture set")


# Read-only reports and settings

def test_current_threshold(initialized_hand):
    assert initialized_hand.init()
    single = initialized_hand.get_current_threshold(1)
    thresholds = initialized_hand.get_all_current_thresholds()
    print(f"\n[GetCurrentThreshold] J1={single}, all={thresholds}")
    if not thresholds:
        return
    assert len(thresholds) == DOF


def test_error_report(initialized_hand):
    assert initialized_hand.init()
    single = initialized_hand.get_error_report(1)
    reports = initialized_hand.get_all_error_reports()
    print(f"\n[GetErrorReport] J1={single}")
    print(f"[GetAllErrorReport] count={len(reports)}")
    assert len(reports) == DOF


def test_temperature_report(initialized_hand):
    assert initialized_hand.init()
    single = initialized_hand.get_temperature_report(1)
    temperatures = initialized_hand.get_all_temperature_reports()
    print(f"\n[GetTemperatureReport] J1={single}, all={temperatures}")
    assert len(temperatures) == DOF


def test_current_report(initialized_hand):
    assert initialized_hand.init()
    single = initialized_hand.get_current_report(1)
    currents = initialized_hand.get_all_current_reports()
    print(f"\n[GetCurrentReport] J1={single}, all={currents}")
    if not currents:
        return
    assert len(currents) == DOF


def test_request_settings(initialized_hand):
    assert initialized_hand.init()
    original_interval = initialized_hand.get_request_interval()
    original_timeout = initialized_hand.get_frame_recv_timeout()
    try:
        initialized_hand.set_request_interval(10)
        assert initialized_hand.get_request_interval() == 10
        initialized_hand.set_frame_recv_timeout(100)
        assert initialized_hand.get_frame_recv_timeout() == 100
    finally:
        initialized_hand.set_request_interval(original_interval)
        initialized_hand.set_frame_recv_timeout(original_timeout)


def test_invalid_joint_index(initialized_hand):
    assert initialized_hand.init()
    assert initialized_hand.get_joint_position(0) == -1
    assert initialized_hand.get_joint_position(DOF + 1) == -1
    assert initialized_hand.get_joint_velocity(0) == -1
    assert initialized_hand.get_joint_velocity(DOF + 1) == -1


def test_constants(initialized_hand):
    assert OmniHand3Lite.kDegreesOfActiveFreedom == DOF


# Tactile sensors

def test_init_tactile_points_map(initialized_hand, tactile_sensors):
    assert tactile_sensors
    print("\n[InitTactilePointsMap] OK")


def test_get_sensor_order(initialized_hand):
    assert initialized_hand.init()
    sensors = list(initialized_hand.get_sensor_order())
    print(f"\n[GetSensorOrder] {sensors}")
    assert sensors


def test_tactile_sensor_data(initialized_hand, tactile_sensors):
    for sensor in tactile_sensors:
        data = initialized_hand.get_tactile_sensor_data(sensor)
        print(f"\n[GetTactileSensorData] {sensor.name}: {data.data[:6]}")
        assert data.sensor_id == sensor


def test_tactile_sensor_data_raw(initialized_hand, tactile_sensors):
    for sensor in tactile_sensors:
        data = initialized_hand.get_tactile_sensor_data_raw(sensor)
        print(f"\n[GetTactileSensorDataRaw] {sensor.name}: {data.data}")
        assert data.sensor_id == sensor


def test_all_tactile_sensor_data_raw(initialized_hand, tactile_sensors):
    all_data = initialized_hand.get_all_tactile_sensor_data_raw()
    print(f"\n[GetAllTactileSensorDataRaw] count={len(all_data)}")
    assert all_data, "GetAllTactileSensorDataRaw returned empty"
    assert len(all_data) == len(tactile_sensors)


def test_num_of_tactile_sensors(initialized_hand):
    assert initialized_hand.init()
    count = initialized_hand.get_num_of_tactile_sensors()
    print(f"\n[GetNumOfTactileSensors] {count}")
    assert count > 0
    assert count == len(initialized_hand.get_sensor_order())


def test_num_of_tactile_points(initialized_hand):
    assert initialized_hand.init()
    for sensor in initialized_hand.get_sensor_order():
        points = initialized_hand.get_num_of_tactile_points(sensor)
        print(f"\n[GetNumOfTactilePoints] {sensor.name}: {points}")
        assert points > 0
    assert initialized_hand.get_num_of_tactile_points(Finger.UNKNOWN) == 0
    assert initialized_hand.get_num_of_tactile_points(Finger.DORSUM) == 0


def test_len_of_tactile_datum(initialized_hand):
    assert initialized_hand.init()
    for sensor in initialized_hand.get_sensor_order():
        length = initialized_hand.get_len_of_tactile_datum(sensor)
        print(f"\n[GetLenOfTactileDatum] {sensor.name}: {length}")
        assert length > 0


def test_num_of_replied_tactile_frames(initialized_hand):
    assert initialized_hand.init()
    for sensor in initialized_hand.get_sensor_order():
        count = initialized_hand.get_num_of_replied_tactile_frames(sensor)
        print(f"\n[GetNumOfRepliedTactileFrames] {sensor.name}: {count}")
        assert count > 0


def test_sn_of_tactile_sensor(initialized_hand):
    assert initialized_hand.init()
    for sensor in initialized_hand.get_sensor_order():
        serial_number = initialized_hand.get_sn_of_tactile_sensor(sensor)
        print(f"\n[GetSNOfTactileSensor] {sensor.name}: {serial_number!r}")


if __name__ == "__main__":
    _export_config_to_environment()
    raise SystemExit(pytest.main([__file__, "-v", "-s", *sys.argv[1:]]))
