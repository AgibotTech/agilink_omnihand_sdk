# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Unified hardware tests for OmniPicker 3 (OP3, 1 DOF).

The suite mirrors the non-destructive public-API coverage in
``cpp/test/test_omnipicker_3.cc``. Device-ID writes, calibration, zeroing,
firmware maintenance, persistent parameter writes, deprecated registers, and
unsupported PV/PVT or kinematics interfaces are deliberately excluded.

Examples:

    python test_omnipicker_3.py -d hcan -i 0 -c 0
    python test_omnipicker_3.py -d rs485 -p COM7
    OMNIHAND_DEVICE_TYPE=hcan pytest -sv test_omnipicker_3.py
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
    OmniPicker3,
    OmniPicker3Gesture,
    ProductType,
    TactileSensorDataU16,
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
FRAME_RECV_TIMEOUT = 200
DEVICE_ID = OmniPicker3.kDefaultHandDeviceId

DOF = 1
MOTION_SETTLE_SECONDS = 1.0
STROKE_SETTLE_SECONDS = 1.5
STROKE_TOLERANCE = 100  # 1.00 mm, because the protocol unit is 0.01 mm.

_STR_OPTIONS = (
    ("OMNIHAND_DEVICE_TYPE", "-d", "DEVICE_TYPE"),
    ("OMNIHAND_CAN_IF", "--can-if", "CAN_IF"),
    ("OMNIHAND_TCP_HOST", "--tcp-host", "TCP_HOST"),
    ("OMNIHAND_RS485_PORT", "-p", "RS485_PORT"),
    ("OMNIHAND_RS485_PORT", "-P", "RS485_PORT"),
)
_INT_OPTIONS = (
    ("OMNIHAND_REQUEST_INTERVAL", "-f", "REQUEST_INTERVAL"),
    ("OMNIHAND_CHANNEL_ID", "-c", "CHANNEL_ID"),
    ("OMNIHAND_CANFD_ID", "-i", "CANFD_ID"),
    ("OMNIHAND_TCP_PORT", "--tcp-port", "TCP_PORT"),
    ("OMNIHAND_BAUDRATE", "-b", "BAUDRATE"),
    ("OMNIHAND_FRAME_RECV_TIMEOUT", "-t", "FRAME_RECV_TIMEOUT"),
    ("OMNIHAND_TARGET_DEVICE_ID", "--device-id", "DEVICE_ID"),
    ("OMNIHAND_TARGET_DEVICE_ID", "--id", "DEVICE_ID"),
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
        os.environ[env_name] = str(globals()[attr])


_apply_environment()
if __name__ == "__main__":
    _apply_direct_cli()

if DEVICE_TYPE not in TRANSPORTS:
    print(f"[Warning] Unknown transport {DEVICE_TYPE!r}; using zlgcan")
    DEVICE_TYPE = "zlgcan"
if not 1 <= DEVICE_ID <= 0x7F:
    raise ValueError("--device-id must be in [1, 0x7f]")
REQUEST_INTERVAL = max(0, min(100, REQUEST_INTERVAL))


def _create_hand():
    common = {
        "hand_type": HandType.UNKNOWN,
        "hand_device_id": DEVICE_ID,
    }
    if DEVICE_TYPE == "hcan":
        return OmniPicker3.create_hand_by_hcan(
            **common,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID,
        )
    if DEVICE_TYPE == "socketcan":
        if not hasattr(OmniPicker3, "create_hand_socketcan"):
            pytest.fail("SocketCAN is unavailable on this platform", pytrace=False)
        return OmniPicker3.create_hand_socketcan(
            **common,
            can_interface=CAN_IF,
        )
    if DEVICE_TYPE == "zlgcantcp":
        if not hasattr(OmniPicker3, "create_hand_by_zlgcan_tcp"):
            pytest.fail("ZLG CAN TCP is unavailable on this platform", pytrace=False)
        return OmniPicker3.create_hand_by_zlgcan_tcp(
            **common,
            host=TCP_HOST,
            port=TCP_PORT,
            canfd_channel_id=CHANNEL_ID,
        )
    if DEVICE_TYPE == "rs485":
        return OmniPicker3.create_hand_by_rs485(
            **common,
            serial_port=RS485_PORT,
            baud_rate=BAUDRATE,
        )
    return OmniPicker3.create_hand_by_zlgcan(
        **common,
        canfd_device_id=CANFD_ID,
        canfd_channel_id=CHANNEL_ID,
    )


@pytest.fixture(scope="session")
def hand():
    try:
        instance = _create_hand()
    except Exception as error:
        pytest.fail(
            f"Failed to create {DEVICE_TYPE} OP3 device: {error}",
            pytrace=False,
        )
    if instance is None:
        pytest.fail(f"{DEVICE_TYPE} factory returned no OP3 device", pytrace=False)

    instance.set_request_interval(REQUEST_INTERVAL)
    instance.set_frame_recv_timeout(FRAME_RECV_TIMEOUT)
    instance.show_data_details(True)
    print(
        f"\n[Info] transport={DEVICE_TYPE} device_id={DEVICE_ID} "
        f"interval={REQUEST_INTERVAL}ms recv_timeout={FRAME_RECV_TIMEOUT}ms"
    )
    if DEVICE_TYPE in ("zlgcan", "hcan", "zlgcantcp"):
        print(f"[Info] canfd_id={CANFD_ID} channel={CHANNEL_ID}")
    elif DEVICE_TYPE == "socketcan":
        print(f"[Info] can_interface={CAN_IF}")
    else:
        print(f"[Info] port={RS485_PORT} baudrate={BAUDRATE}")
    yield instance


@pytest.fixture(scope="session")
def initialized_hand(hand):
    if not hand.init():
        pytest.fail(
            f"{DEVICE_TYPE} OP3 device is not connected or initialization failed",
            pytrace=False,
        )
    yield hand
    try:
        hand.set_hand_gesture(OmniPicker3Gesture.OMNIPICKER_3_GESTURE_ZERO)
        time.sleep(MOTION_SETTLE_SECONDS)
    except Exception as error:
        print(f"[Warning] Failed to return OP3 to ZERO: {error}")


@pytest.fixture(scope="session")
def tactile_sensors(initialized_hand):
    if not initialized_hand.init_tactile_points_map():
        pytest.fail("OP3 tactile sensor initialization failed", pytrace=False)
    sensors = initialized_hand.get_sensor_order()
    if not sensors:
        pytest.fail("OP3 returned an empty tactile sensor order", pytrace=False)
    return sensors


def test_safe_api_surface():
    assert OmniPicker3.kDegreesOfActiveFreedom == DOF
    assert OmniPicker3Gesture.OMNIPICKER_3_GESTURE_FULL_OPEN.value == 2
    assert TactileSensorDataU16 is not None

    for excluded_name in (
        "set_device_id",
        "set_hand_device_id_by_broadcast",
        "calibrate_tactile_sensor",
        "set_zero_point",
        "update_firmware",
        "get_sensor_data_length",
        "set_current_threshold",
        "get_joint_velocity",
        "mix_control_by_pv",
        "mix_control_by_pvt",
    ):
        assert not hasattr(OmniPicker3, excluded_name)


def test_static_limits():
    position = OmniPicker3.get_min_max_motor_position()
    assert (position.min_value, position.max_value) == (0, 4095)

    torque = OmniPicker3.get_min_max_mix_ctrl_torque()
    assert (torque.min_value, torque.max_value, torque.default_value) == (
        0, 6500, 3000
    )

    stroke = OmniPicker3.get_min_max_stroke()
    assert (stroke.min_value, stroke.max_value) == (-500, 11200)

    speed = OmniPicker3.get_min_max_stroke_mix_speed()
    assert (speed.min_value, speed.max_value, speed.default_value) == (
        0, 5000, 3000
    )

    current = OmniPicker3.get_min_max_mix_ctrl_current()
    assert (current.min_value, current.max_value, current.default_value) == (
        0, 3500, 3000
    )


def test_create_and_init(initialized_hand):
    assert initialized_hand.init()
    assert initialized_hand.get_product_type() == ProductType.OMNI_PICKER_3
    assert initialized_hand.get_num_of_joint_motors() == DOF
    assert initialized_hand.get_doa() == DOF
    assert initialized_hand.get_joint_names() == ["joint1"]


def test_vendor_and_device_info(initialized_hand):
    vendor = initialized_hand.get_vendor_info()
    device = initialized_hand.get_device_info()
    print(f"\n[GetVendorInfo]\n{vendor}")
    print(f"[GetDeviceInfo]\n{device}")
    assert vendor.dof == DOF
    assert device.hand_device_id == DEVICE_ID


def test_discover_device_id(initialized_hand):
    discovered = initialized_hand.get_hand_device_id_by_broadcast()
    print(f"\n[GetHandDeviceIdByBroadcast] {discovered}")
    assert discovered > 0
    assert discovered == DEVICE_ID


def test_communication_settings(initialized_hand):
    interval = initialized_hand.get_request_interval()
    recv_timeout = initialized_hand.get_frame_recv_timeout()
    send_timeout = initialized_hand.get_frame_send_timeout()
    try:
        initialized_hand.set_request_interval(min(interval + 1, 100))
        assert initialized_hand.get_request_interval() == min(interval + 1, 100)
        initialized_hand.set_frame_recv_timeout(min(recv_timeout + 10, 1000))
        assert initialized_hand.get_frame_recv_timeout() == min(
            recv_timeout + 10, 1000
        )
        initialized_hand.set_frame_send_timeout(send_timeout)
        assert initialized_hand.get_frame_send_timeout() == send_timeout
    finally:
        initialized_hand.set_request_interval(interval)
        initialized_hand.set_frame_recv_timeout(recv_timeout)


def test_single_joint_position(initialized_hand):
    set_result = initialized_hand.set_joint_position(1, 1024)
    time.sleep(MOTION_SETTLE_SECONDS)
    position = initialized_hand.get_joint_position(1)
    print(f"\n[SinglePosition] set_result={set_result}, readback={position}")
    limits = OmniPicker3.get_min_max_motor_position()
    assert limits.min_value <= position <= limits.max_value


def test_all_joint_positions(initialized_hand):
    set_result = initialized_hand.set_all_joint_positions([2048])
    assert len(set_result) == DOF
    time.sleep(MOTION_SETTLE_SECONDS)
    positions = initialized_hand.get_all_joint_positions()
    print(f"\n[AllPositions] set={set_result}, readback={positions}")
    assert len(positions) == DOF


def test_invalid_joint_index(initialized_hand):
    assert initialized_hand.get_joint_position(0) == -1
    assert initialized_hand.get_joint_position(DOF + 1) == -1
    assert initialized_hand.set_joint_position(0, 1024) == -1
    assert initialized_hand.set_joint_position(DOF + 1, 1024) == -1


def test_stroke_control(initialized_hand):
    target = 5000
    first = initialized_hand.mix_control_by_stroke(target)
    print(f"\n[MixControlByStroke] stroke-only reply={first}")
    time.sleep(STROKE_SETTLE_SECONDS)

    full = initialized_hand.mix_control_by_stroke(target, 3000, 3000)
    print(f"[MixControlByStroke] full reply={full}")
    assert full == pytest.approx(target, abs=STROKE_TOLERANCE)
    time.sleep(0.5)

    partial = initialized_hand.mix_control_by_stroke(target, current=2000)
    print(f"[MixControlByStroke] current-only reply={partial}")
    assert partial == pytest.approx(target, abs=STROKE_TOLERANCE)


def test_stroke_and_current_clamping(initialized_hand):
    initialized_hand.mix_control_by_stroke(12000)
    time.sleep(STROKE_SETTLE_SECONDS)
    at_limit = initialized_hand.mix_control_by_stroke(12000)
    maximum = OmniPicker3.get_min_max_stroke().max_value
    print(f"\n[StrokeClamp] settled={at_limit}, maximum={maximum}")
    assert at_limit == pytest.approx(maximum, abs=STROKE_TOLERANCE)
    assert at_limit <= maximum

    current_reply = initialized_hand.mix_control_by_stroke(5000, 3000, 5000)
    assert current_reply != -1
    time.sleep(STROKE_SETTLE_SECONDS)


def test_gentle_grip(initialized_hand):
    default_reply = initialized_hand.set_gentle_grip()
    print(f"\n[SetGentleGrip] default={default_reply}")
    assert len(default_reply) == 4
    time.sleep(0.5)

    limited_reply = initialized_hand.set_gentle_grip(1000)
    print(f"[SetGentleGrip] limited={limited_reply}")
    assert len(limited_reply) == 4
    assert limited_reply[0] == 1
    assert limited_reply[2] | (limited_reply[3] << 8) == 1000


def test_hand_gestures(initialized_hand):
    gestures = (
        OmniPicker3Gesture.OMNIPICKER_3_GESTURE_ZERO,
        OmniPicker3Gesture.OMNIPICKER_3_GESTURE_HALF_OPEN,
        OmniPicker3Gesture.OMNIPICKER_3_GESTURE_FULL_OPEN,
    )
    for gesture in gestures:
        target = initialized_hand.get_hand_gesture(gesture)
        assert len(target) == DOF
        initialized_hand.set_hand_gesture(gesture)
        time.sleep(MOTION_SETTLE_SECONDS)
        readback = initialized_hand.get_all_joint_positions()
        print(f"\n[Gesture] {gesture.name}: target={target}, readback={readback}")
        assert len(readback) == DOF


def test_error_report(initialized_hand):
    single = initialized_hand.get_error_report(1)
    reports = initialized_hand.get_all_error_reports()
    print(f"\n[GetErrorReport] single={single}, count={len(reports)}")
    assert len(reports) == DOF


def test_temperature_report(initialized_hand):
    single = initialized_hand.get_temperature_report(1)
    temperatures = initialized_hand.get_all_temperature_reports()
    print(f"\n[GetTemperatureReport] single={single}, all={temperatures}")
    assert len(temperatures) == DOF
    assert all(-40 <= temperature <= 127 for temperature in temperatures)


def test_current_report(initialized_hand):
    single = initialized_hand.get_current_report(1)
    currents = initialized_hand.get_all_current_reports()
    print(f"\n[GetCurrentReport] single={single}, all={currents}")
    assert len(currents) == DOF


def test_mix_control_by_pt(initialized_hand):
    current = OmniPicker3.get_min_max_mix_ctrl_torque().default_value
    result = initialized_hand.mix_control_by_pt([2048], [current])
    print(f"\n[MixControlByPT] result={result}")
    assert len(result) == DOF
    assert result[0].tgt_posi is not None
    time.sleep(MOTION_SETTLE_SECONDS)
    assert len(initialized_hand.get_all_joint_positions()) == DOF


def test_tactile_metadata(initialized_hand, tactile_sensors):
    assert initialized_hand.get_num_of_tactile_sensors() == len(tactile_sensors)
    for sensor in tactile_sensors:
        points = initialized_hand.get_num_of_tactile_points(sensor)
        datum_length = initialized_hand.get_len_of_tactile_datum(sensor)
        frames = initialized_hand.get_num_of_replied_tactile_frames(sensor)
        serial_number = initialized_hand.get_sn_of_tactile_sensor(sensor)
        print(
            f"\n[TactileMetadata] {sensor.name}: points={points}, "
            f"datum={datum_length}, frames={frames}, sn={serial_number!r}"
        )
        assert points > 0
        assert datum_length > 0
        assert frames > 0

    assert initialized_hand.get_num_of_tactile_points(Finger.UNKNOWN) == 0
    assert initialized_hand.get_len_of_tactile_datum(Finger.UNKNOWN) == 0
    assert initialized_hand.get_num_of_replied_tactile_frames(Finger.UNKNOWN) == 0
    assert initialized_hand.get_sn_of_tactile_sensor(Finger.UNKNOWN) == ""


def test_tactile_sensor_data_raw(initialized_hand, tactile_sensors):
    for sensor in tactile_sensors:
        data = initialized_hand.get_tactile_sensor_data_raw(sensor)
        print(f"\n[GetTactileSensorDataRaw] {sensor.name}: {len(data.data)} points")
        assert data.sensor_id == sensor
        assert len(data.data) == initialized_hand.get_num_of_tactile_points(sensor)


def test_all_tactile_sensor_data_raw(initialized_hand, tactile_sensors):
    all_data = initialized_hand.get_all_tactile_sensor_data_raw()
    print(f"\n[GetAllTactileSensorDataRaw] count={len(all_data)}")
    assert len(all_data) == len(tactile_sensors)
    assert [data.sensor_id for data in all_data] == tactile_sensors
    for data in all_data:
        assert len(data.data) == initialized_hand.get_num_of_tactile_points(
            data.sensor_id
        )


if __name__ == "__main__":
    _export_config_to_environment()
    raise SystemExit(pytest.main([__file__, "-v", "-s", *sys.argv[1:]]))
