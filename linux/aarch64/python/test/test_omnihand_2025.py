# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""
Unified hardware tests for OmniHand 2025 (O10): CANFD transports + USB/RS485.

Merged from test_omnihand_2025.py / test_omnihand_2025_canfd.py /
test_omnihand_2025_usb.py / test_omnihand_2025_zlgcan_tcp.py.

Transport is selected with -d (or OMNIHAND_DEVICE_TYPE):
  zlgcan | hcan | socketcan | zlgcantcp | usb | rs485   (default: zlgcan)

Usage:
  python3 test_omnihand_2025.py [options]
  OMNIHAND_DEVICE_TYPE=hcan python3 -m pytest test_omnihand_2025.py

Common options:
  -d TRANSPORT   Backend: zlgcan | hcan | socketcan | zlgcantcp | usb | rs485
                 (default: zlgcan)
  -f INTERVAL    Request interval ms. CAN default 5 (max 100), USB/RS485
                 default 500 (max 500). 0 disables interval limiting.
  --device-id ID Optional target hand ID in [1, 0x7f] (alias: --id). When
                 omitted, only discover/cache current IDs; no ID is written.

CAN options (zlgcan / hcan / socketcan / zlgcantcp):
  -c CHANNEL     CAN channel index (zlgcan/hcan/zlgcantcp), default 0
  -i CANFD_ID    Adapter device index (zlgcan/hcan), default 0
  --can-if IF    SocketCAN interface name (socketcan), default can0
  --tcp-host H   ZLG CANFD-over-TCP host (zlgcantcp), default 192.168.0.178
  --tcp-port P   TCP port (zlgcantcp), default 8000

USB/RS485 options:
  -p PORT        Serial port (USB default: /dev/ttyACM0, Windows COM3;
                 RS485 default: /dev/ttyUSB0, Windows COM5)
  -b BAUDRATE    Baudrate (default: 460800)
  -t TIMEOUT     Frame receive timeout in ms (default: 200)

Environment variables (take priority over defaults, work with pytest):
  OMNIHAND_DEVICE_TYPE, OMNIHAND_REQUEST_INTERVAL, OMNIHAND_CHANNEL_ID,
  OMNIHAND_CANFD_ID, OMNIHAND_CAN_IF, OMNIHAND_TCP_HOST, OMNIHAND_TCP_PORT,
  OMNIHAND_USB_PORT, OMNIHAND_RS485_PORT, OMNIHAND_BAUDRATE,
  OMNIHAND_FRAME_RECV_TIMEOUT, OMNIHAND_TARGET_DEVICE_ID

Examples:
  python3 test_omnihand_2025.py -d zlgcan -c 0 -i 0
  python3 test_omnihand_2025.py -d socketcan --can-if can0
  python3 test_omnihand_2025.py -d zlgcantcp --tcp-host 192.168.0.178 --tcp-port 8000
  python3 test_omnihand_2025.py -d usb -p /dev/ttyACM0
  python3 test_omnihand_2025.py -d rs485 -p /dev/ttyUSB0
  OMNIHAND_DEVICE_TYPE=usb pytest -v test_omnihand_2025.py
"""

import os
import platform
import sys
import time

# Ensure we use the installed omnihand package, not the source directory
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir in sys.path:
    sys.path.remove(parent_dir)

try:
    import pytest
except ImportError:
    print("Python tests require pytest. Install with: pip install pytest")
    sys.exit(1)

from omnihand import OmniHand2025, Finger, HandType

# ---------------------------------------------------------------------------
# Configuration (env vars take priority over defaults, CLI args parsed below)
# ---------------------------------------------------------------------------
CAN_TRANSPORTS = ("zlgcan", "hcan", "socketcan", "zlgcantcp")
TRANSPORTS = CAN_TRANSPORTS + ("usb", "rs485")

DEVICE_TYPE = "zlgcan"
CHANNEL_ID = 0
CANFD_ID = 0
# None = use transport default (CAN: 5ms, USB/RS485: 500ms)
REQUEST_INTERVAL = None
CAN_IF = "can0"
TCP_HOST = "192.168.0.178"
TCP_PORT = 8000
if platform.system() == "Windows":
    USB_PORT = "COM3"
    RS485_PORT = "COM5"
else:
    USB_PORT = "/dev/ttyACM0"
    RS485_PORT = "/dev/ttyUSB0"
BAUDRATE = 460800
FRAME_RECV_TIMEOUT = 200
TARGET_DEVICE_ID = None

# (env var, CLI flag, module attribute)
_STR_OPTIONS = (
    ("OMNIHAND_DEVICE_TYPE", "-d", "DEVICE_TYPE"),
    ("OMNIHAND_CAN_IF", "--can-if", "CAN_IF"),
    ("OMNIHAND_TCP_HOST", "--tcp-host", "TCP_HOST"),
    ("OMNIHAND_USB_PORT", "-p", "USB_PORT"),
    ("OMNIHAND_RS485_PORT", "-p", "RS485_PORT"),
)
_INT_OPTIONS = (
    ("OMNIHAND_CHANNEL_ID", "-c", "CHANNEL_ID"),
    ("OMNIHAND_CANFD_ID", "-i", "CANFD_ID"),
    ("OMNIHAND_TCP_PORT", "--tcp-port", "TCP_PORT"),
    ("OMNIHAND_BAUDRATE", "-b", "BAUDRATE"),
    ("OMNIHAND_FRAME_RECV_TIMEOUT", "-t", "FRAME_RECV_TIMEOUT"),
    ("OMNIHAND_REQUEST_INTERVAL", "-f", "REQUEST_INTERVAL"),
    ("OMNIHAND_TARGET_DEVICE_ID", "--device-id", "TARGET_DEVICE_ID"),
    ("OMNIHAND_TARGET_DEVICE_ID", "--id", "TARGET_DEVICE_ID"),
)


def _apply_env():
    for env_name, _flag, attr in _STR_OPTIONS:
        value = os.environ.get(env_name)
        if value:
            globals()[attr] = value
    for env_name, _flag, attr in _INT_OPTIONS:
        value = os.environ.get(env_name)
        if value is not None:
            try:
                globals()[attr] = int(value)
            except ValueError:
                print(f"[Warning]: Invalid {env_name} value: {value}")


def _apply_cli_args():
    # Only takes effect when running directly with Python; the flags are
    # stripped from sys.argv so pytest.main() does not see them.
    argv = sys.argv[:]
    for options, cast in ((_STR_OPTIONS, str), (_INT_OPTIONS, int)):
        for _env_name, flag, attr in options:
            if flag == "-p":
                # Serial port for USB (/dev/ttyACM0) and RS485 (/dev/ttyUSB0);
                # like the C++ test, -p targets the transport selected by -d.
                attr = "RS485_PORT" if DEVICE_TYPE == "rs485" else "USB_PORT"
            if flag in argv:
                idx = argv.index(flag)
                if idx + 1 < len(argv):
                    try:
                        globals()[attr] = cast(argv[idx + 1])
                    except ValueError:
                        print(f"[Warning]: Invalid {flag} value: {argv[idx + 1]}")
                    argv.pop(idx)
                    argv.pop(idx)
    sys.argv = argv


_apply_env()
if DEVICE_TYPE not in TRANSPORTS:
    print(f"[Warning]: OMNIHAND_DEVICE_TYPE must be one of {TRANSPORTS}, "
          f"got: {DEVICE_TYPE}; using zlgcan")
    DEVICE_TYPE = "zlgcan"
_apply_cli_args()
if TARGET_DEVICE_ID is not None and not 1 <= TARGET_DEVICE_ID <= 0x7F:
    raise ValueError("--device-id must be in [1, 0x7f]")


def is_serial_transport():
    """True for the USB/RS485 serial transports (private protocol)."""
    return DEVICE_TYPE in ("usb", "rs485")


def effective_request_interval():
    """Request interval: CAN default 5ms (max 100), USB/RS485 default 500ms (max 500)."""
    fallback, max_value = (500, 500) if is_serial_transport() else (5, 100)
    value = fallback if REQUEST_INTERVAL is None else REQUEST_INTERVAL
    return max(0, min(max_value, value))


def serial_port():
    """Serial port of the selected USB/RS485 transport."""
    return RS485_PORT if DEVICE_TYPE == "rs485" else USB_PORT


# ---------------------------------------------------------------------------
# Session-scoped fixtures (one device for the whole test run)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def hand():
    """Create one OmniHand 2025 instance for the selected transport."""
    if DEVICE_TYPE == "hcan":
        h = OmniHand2025.create_hand_by_hcan(
            hand_type=HandType.LEFT,
            hand_device_id=0,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID,
        )
    elif DEVICE_TYPE == "socketcan":
        h = OmniHand2025.create_hand_socketcan(
            hand_type=HandType.LEFT,
            hand_device_id=0,
            can_interface=CAN_IF,
        )
    elif DEVICE_TYPE == "zlgcantcp":
        h = OmniHand2025.create_hand_by_zlgcan_tcp(
            hand_type=HandType.LEFT,
            hand_device_id=0,
            host=TCP_HOST,
            port=TCP_PORT,
            canfd_channel_id=CHANNEL_ID,
        )
    elif DEVICE_TYPE in ("usb", "rs485"):
        port = serial_port()
        try:
            if DEVICE_TYPE == "usb":
                h = OmniHand2025.create_hand_by_usb(
                    hand_type=HandType.LEFT,
                    hand_device_id=0,
                    uart_port=port,
                    baudrate=BAUDRATE,
                )
            else:
                h = OmniHand2025.create_hand_by_rs485(
                    hand_type=HandType.LEFT,
                    hand_device_id=0,
                    uart_port=port,
                    baudrate=BAUDRATE,
                )
        except Exception as e:
            pytest.fail(f"Failed to open {DEVICE_TYPE} port {port}: {e}",
                        pytrace=False)
    else:  # zlgcan (default)
        h = OmniHand2025.create_hand_by_zlgcan(
            hand_type=HandType.LEFT,
            hand_device_id=0,
            canfd_device_id=CANFD_ID,
            canfd_channel_id=CHANNEL_ID,
        )
    h.set_request_interval(effective_request_interval())
    h.set_frame_recv_timeout(FRAME_RECV_TIMEOUT)
    h.show_data_details(True)
    print(f"\n[Info] transport={DEVICE_TYPE} channel={CHANNEL_ID} canfd_id={CANFD_ID} "
          f"interval={effective_request_interval()}ms")
    if is_serial_transport():
        print(f"[Info] port={serial_port()} baudrate={BAUDRATE} "
              f"timeout={FRAME_RECV_TIMEOUT}ms")
    yield h


@pytest.fixture(scope="session")
def initialized_hand(hand):
    """Initialize, discover IDs, and optionally perform an explicit ID test."""
    if not hand.init():
        pytest.fail(f"{DEVICE_TYPE} device not available", pytrace=False)

    original_device_id = None
    id_change_attempted = False
    try:
        original_device_id = hand.get_hand_device_id_by_broadcast()
        assert original_device_id >= 0, (
            "No consistent device ID responded to broadcast"
        )
        print(f"\n[Info] Discovered device ID state: {original_device_id}")

        if TARGET_DEVICE_ID is not None:
            print(
                f"[Info] Switching device ID state: "
                f"{original_device_id} -> {TARGET_DEVICE_ID}"
            )
            id_change_attempted = True
            assert hand.set_hand_device_id_by_broadcast(TARGET_DEVICE_ID)
        yield hand
    finally:
        if id_change_attempted:
            print(
                f"\n[Info] Restoring device ID state: "
                f"{TARGET_DEVICE_ID} -> {original_device_id}"
            )
            assert hand.set_hand_device_id_by_broadcast(original_device_id)


# ---------------------------------------------------------------------------
# Basic connection
# ---------------------------------------------------------------------------

def test_create_hand(hand):
    """Test factory method for the selected transport."""
    assert hand is not None


def test_init(initialized_hand):
    """Test initialization (idempotent re-init after fixture succeeded)."""
    assert initialized_hand.init()


# ---------------------------------------------------------------------------
# Vendor / device info
# ---------------------------------------------------------------------------

def test_get_vendor_info(initialized_hand):
    """Test vendor info (O10 has 10 DOF)."""
    info = initialized_hand.get_vendor_info()
    print(f"\n[get_vendor_info]\n{info}")
    assert info.dof != 0, "GetVendorInfo timed out"
    assert info.dof == 10
    assert info.product_model


def test_get_device_info(initialized_hand):
    """Test device info."""
    info = initialized_hand.get_device_info()
    print(f"\n[get_device_info]\n{info}")
    assert info.hand_device_id != 0, "GetDeviceInfo timed out"
    discovered_id = initialized_hand.get_hand_device_id_by_broadcast()
    assert discovered_id >= 0
    expected_id = (
        OmniHand2025.kDefaultHandDeviceId
        if discovered_id == 0 else discovered_id
    )
    assert info.hand_device_id == expected_id


# ---------------------------------------------------------------------------
# Position control
# ---------------------------------------------------------------------------

def test_set_get_single_axis_pos(initialized_hand):
    """Test single-joint position set/get for all 10 joints."""
    safe_pos = [1024, 1024, 2048, 1024, 2048, 2048, 1024, 2048, 1024, 2048]
    print("\n[SetGetSingleAxisPos] Testing all 10 joints:")
    for joint_idx, target in enumerate(safe_pos, start=1):
        initialized_hand.set_joint_position(joint_idx, target)
        time.sleep(1.0)
        pos = initialized_hand.get_joint_position(joint_idx)
        print(f"  J{joint_idx}: target={target}, get={pos}")
        assert 0 <= pos <= 4096


def test_set_get_all_axis_pos(initialized_hand):
    """Test batch position set/get for all 10 joints."""
    positions = [2048, 2048, 4096, 0, 4096, 4096, 0, 4096, 0, 4096]
    set_result = initialized_hand.set_all_joint_positions(positions)
    print(f"\n[SetAllJointMotorPosi] returned {len(set_result)} positions: {set_result}")
    assert len(set_result) == 10
    time.sleep(1.0)

    get_result = initialized_hand.get_all_joint_positions()
    print(f"[GetAllJointMotorPosi] returned {len(get_result)} positions: {get_result}")
    assert len(get_result) == 10


# ---------------------------------------------------------------------------
# Current / velocity / temperature / error reports
# ---------------------------------------------------------------------------

def test_get_all_current_report(initialized_hand):
    """Test current report (mA) for all joints."""
    currents = initialized_hand.get_all_current_reports()
    print("\n[GetAllCurrentReport] " + ", ".join(f"J{i + 1}:{v}mA" for i, v in enumerate(currents)))
    assert len(currents) == 10, "GetAllCurrentReport timed out or returned an invalid size"


def test_get_current_report(initialized_hand):
    """Test per-joint current queries."""
    print("\n[GetCurrentReport] All joints:")
    for joint in range(1, 11):
        current = initialized_hand.get_current_report(joint)
        print(f"  J{joint}: {current}mA")


def test_get_all_temperature_report(initialized_hand):
    """Test temperature report for all joints (int8 range: -40 to 127 degC)."""
    temps = initialized_hand.get_all_temperature_reports()
    print("\n[GetAllTemperatureReport] " + ", ".join(f"J{i + 1}:{v}degC" for i, v in enumerate(temps)))
    assert len(temps) == 10, "GetAllTemperatureReport timed out or returned an invalid size"
    for t in temps:
        assert -40 <= t <= 127


def test_get_temperature_report(initialized_hand):
    """Test per-joint temperature queries."""
    print("\n[GetTemperatureReport] All joints:")
    for joint in range(1, 11):
        temperature = initialized_hand.get_temperature_report(joint)
        print(f"  J{joint}: {temperature}degC")
        assert -40 <= temperature <= 127


def test_get_all_error_report(initialized_hand):
    """Test error report for all joints."""
    errors = initialized_hand.get_all_error_reports()
    flags = (
        ("stalled", "S"),
        ("overheat", "H"),
        ("over_current", "C"),
        ("motor_except", "M"),
        ("commu_except", "X"),
    )
    parts = []
    for i, e in enumerate(errors):
        flag_str = "".join(code for name, code in flags if getattr(e, name, False))
        parts.append(f"J{i + 1}:[{flag_str}]")
    print("\n[GetAllErrorReport] " + " ".join(parts))
    assert len(errors) == 10, "GetAllErrorReport timed out or returned an invalid size"


def test_get_error_report(initialized_hand):
    """Test per-joint error queries."""
    print("\n[GetErrorReport] All joints:")
    for joint in range(1, 11):
        error = initialized_hand.get_error_report(joint)
        flags = "".join(
            code for name, code in (
                ("stalled", "S"),
                ("overheat", "H"),
                ("over_current", "C"),
                ("motor_except", "M"),
                ("commu_except", "X"),
            ) if getattr(error, name, False)
        )
        print(f"  J{joint}: [{flags}]")


def test_get_all_joint_motor_velo(initialized_hand):
    """Test velocity report for all joints."""
    velos = initialized_hand.get_all_joint_velocities()
    print("\n[GetAllJointMotorVelo] " + ", ".join(str(v) for v in velos))
    assert len(velos) == 10, "GetAllJointMotorVelo timed out or returned an invalid size"


def test_get_joint_motor_velo(initialized_hand):
    """Test per-joint velocity queries and CAN index validation."""
    print("\n[GetJointMotorVelo] Per joint:")
    for joint in range(1, 11):
        print(f"  J{joint}: velo={initialized_hand.get_joint_velocity(joint)}")
    if not is_serial_transport():
        assert initialized_hand.get_joint_velocity(0) == -1
        assert initialized_hand.get_joint_velocity(11) == -1


def test_get_all_current_threshold(initialized_hand):
    """Test current threshold for all joints (USB returns cached values)."""
    thresholds = initialized_hand.get_all_current_thresholds()
    # Empty by design when unsupported: USB private protocol has no GET command
    # and CAN firmware without register 0x03 support times out.
    if not thresholds:
        pytest.skip("GetAllCurrentThreshold not supported by this firmware/transport")
    print(f"\n[GetAllCurrentThreshold] {thresholds}")
    assert len(thresholds) == 10


# ---------------------------------------------------------------------------
# Tactile sensors
# ---------------------------------------------------------------------------

def test_get_tactile_sensor_data(initialized_hand):
    """Test downsampled tactile data: fingers 16 values, palm/dorsum 25 values."""
    fingers = [Finger.THUMB, Finger.INDEX, Finger.MIDDLE, Finger.RING, Finger.LITTLE]
    print("\n[GetTactileSensorData] Fingers (16 values each):")
    for finger in fingers:
        data = initialized_hand.get_tactile_sensor_data(finger)
        print(f"  {finger.name}: {list(data)}")
        assert len(data) == 16

    print("[GetTactileSensorData] Palm/Dorsum (25 values each):")
    for sensor in (Finger.PALM, Finger.DORSUM):
        data = initialized_hand.get_tactile_sensor_data(sensor)
        print(f"  {sensor.name}: {list(data)}")
        assert len(data) == 25


def test_get_all_tactile_sensor_data(initialized_hand):
    """Test all seven downsampled tactile sensors in one call."""
    all_data = initialized_hand.get_all_tactile_sensor_data()
    print(f"\n[GetAllTactileSensorData] {len(all_data)} sensors:")
    assert len(all_data) == 7
    for sensor in all_data:
        print(f"  {sensor.sensor_id.name}: {len(sensor.data)} bytes")
        assert sensor.data


def test_get_tactile_sensor_data_raw(initialized_hand):
    """Test one raw tactile sensor (full resolution)."""
    data = initialized_hand.get_tactile_sensor_data_raw(Finger.THUMB)
    if not data.data:
        pytest.skip("Raw tactile data not supported by this firmware")
    print(f"\n[GetTactileSensorDataRaw] Thumb ({len(data.data)} values): "
          f"{[int(x) for x in data.data[:10]]}...")
    assert len(data.data) > 0


def test_get_all_tactile_sensor_data_raw(initialized_hand):
    """Test raw tactile data for all sensors."""
    all_data = initialized_hand.get_all_tactile_sensor_data_raw()
    if not all_data:
        pytest.skip("Raw tactile data not supported by this firmware")
    print(f"\n[GetAllTactileSensorDataRaw] {len(all_data)} sensors:")
    for sensor in all_data:
        print(f"  {sensor.sensor_id.name}: {len(sensor.data)} points")
    assert all_data


# ---------------------------------------------------------------------------
# Mixed control
# ---------------------------------------------------------------------------

def test_mix_control_by_pvt(initialized_hand):
    """Test PVT mixed control for all joints and single joint."""
    safe_pos = [2048, 2048, 4096, 2048, 4096, 4096, 2048, 4096, 2048, 4096]
    velocities = [50] * 10 if is_serial_transport() else [8000] * 10
    torques = [0] * 10 if is_serial_transport() else [300] * 10
    result = initialized_hand.mix_control_by_pvt(safe_pos, velocities, torques)
    assert len(result) == len(safe_pos)
    print("\n[MixControlByPVT] all 10 joints")
    time.sleep(1.0)
    feedback = initialized_hand.get_all_joint_positions()
    print(f"[GetAllJointMotorPosi] {feedback}")
    assert len(feedback) == 10

    if not is_serial_transport():
        single_result = initialized_hand.mix_control_by_pvt(1, safe_pos[0], 50, 0)
        assert single_result.tgt_posi is not None
        assert single_result.tgt_velo is not None
        assert single_result.tgt_torque is not None
        assert single_result.joint_index == 1
        print(f"[MixControlByPVT] single J1 pos={safe_pos[0]} vel=50")
        time.sleep(0.5)


def test_mix_control_by_pt(initialized_hand):
    """Test PT mixed control for all joints and single joint."""
    safe_pos = [2048, 2048, 4096, 2048, 4096, 4096, 2048, 4096, 2048, 4096]
    torques = [0] * 10
    result = initialized_hand.mix_control_by_pt(safe_pos, torques)
    assert len(result) == len(safe_pos)
    print("\n[MixControlByPT] all 10 joints")
    time.sleep(1.0)
    if not is_serial_transport():
        single_result = initialized_hand.mix_control_by_pt(1, safe_pos[0], 0)
        assert single_result.tgt_posi is not None
        assert single_result.tgt_torque is not None
        assert single_result.joint_index == 1
        print(f"[MixControlByPT] single J1 pos={safe_pos[0]}")
        time.sleep(0.5)


# ---------------------------------------------------------------------------
# Joint angle control
# ---------------------------------------------------------------------------

def test_set_get_all_active_joint_angles(initialized_hand):
    """Test setting/reading active joint angles (rad)."""
    angles = [0.0] * 10
    initialized_hand.set_all_active_joint_angles(angles)
    print("\n[SetAllActiveJointAngles] All joints -> 0.0 rad")
    time.sleep(1.0)
    current = initialized_hand.get_all_active_joint_angles()
    print(f"[GetAllActiveJointAngles] {[f'{v:.4f}' for v in current]}")
    assert len(current) == 10


def test_get_all_joint_angles(initialized_hand):
    """Test all joint angles (10 active + 6 passive)."""
    all_angles = initialized_hand.get_all_joint_angles()
    print(f"\n[GetAllJointAngles] ({len(all_angles)} joints): {[f'{v:.4f}' for v in all_angles]}")
    assert len(all_angles) == 16


# ---------------------------------------------------------------------------
# Kinematics solver
# ---------------------------------------------------------------------------

def test_kinematics_solver(initialized_hand):
    """Test forward kinematics (10 active angles -> 16 joint angles)."""
    active_angles = [0.0] * 10
    all_angles = initialized_hand.get_all_joint_angles(active_angles)
    print(f"\n[KinematicsSolver] Forward kinematics: {len(all_angles)} angles")
    assert len(all_angles) == 16


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    # Propagate parsed CLI options as env vars so pytest's reimport of this
    # module picks them up (module-level sys.argv parsing runs twice: once here
    # and once when pytest collects the file, at which point the flags are
    # already stripped from sys.argv).
    os.environ["OMNIHAND_DEVICE_TYPE"] = DEVICE_TYPE
    os.environ["OMNIHAND_REQUEST_INTERVAL"] = str(effective_request_interval())
    os.environ["OMNIHAND_CHANNEL_ID"] = str(CHANNEL_ID)
    os.environ["OMNIHAND_CANFD_ID"] = str(CANFD_ID)
    os.environ["OMNIHAND_CAN_IF"] = CAN_IF
    os.environ["OMNIHAND_TCP_HOST"] = TCP_HOST
    os.environ["OMNIHAND_TCP_PORT"] = str(TCP_PORT)
    os.environ["OMNIHAND_USB_PORT"] = USB_PORT
    os.environ["OMNIHAND_RS485_PORT"] = RS485_PORT
    os.environ["OMNIHAND_BAUDRATE"] = str(BAUDRATE)
    os.environ["OMNIHAND_FRAME_RECV_TIMEOUT"] = str(FRAME_RECV_TIMEOUT)
    if TARGET_DEVICE_ID is not None:
        os.environ["OMNIHAND_TARGET_DEVICE_ID"] = str(TARGET_DEVICE_ID)
    else:
        os.environ.pop("OMNIHAND_TARGET_DEVICE_ID", None)

    print("=== OmniHand 2025 Test ===")
    print(f"Transport:        {DEVICE_TYPE}")
    if DEVICE_TYPE in ("zlgcan", "hcan", "zlgcantcp"):
        print(f"Channel ID:       {CHANNEL_ID}")
    if DEVICE_TYPE in ("zlgcan", "hcan"):
        print(f"CANFD ID:         {CANFD_ID}")
    if DEVICE_TYPE == "socketcan":
        print(f"CAN Interface:    {CAN_IF}")
    if DEVICE_TYPE == "zlgcantcp":
        print(f"TCP:              {TCP_HOST}:{TCP_PORT}")
    if is_serial_transport():
        print(f"Port:             {serial_port()}")
        print(f"Baudrate:         {BAUDRATE}")
        print(f"Frame Timeout:    {FRAME_RECV_TIMEOUT} ms")
    print(f"Request Interval: {effective_request_interval()} ms")
    print(
        "Target Device ID: "
        + (str(TARGET_DEVICE_ID) if TARGET_DEVICE_ID is not None
           else "discover only")
    )
    print("==========================")

    args = sys.argv[1:]
    if "-v" not in args and "--verbose" not in args:
        args = ["-v"] + args
    if "-s" not in args and "--capture=no" not in args:
        args = ["-s"] + args
    pytest.main([__file__] + args)
