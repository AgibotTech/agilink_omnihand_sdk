# Copyright (c) 2026, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""H3UM demo: pinky PIP motion, motor feedback, and revision-specific tactile data.

Examples:
    python demo_omnihand_3_ultra_m.py -d zlgcan -i 0 -c 0
    python demo_omnihand_3_ultra_m.py -d hcan -i 0 -c 1 --hand right
    python demo_omnihand_3_ultra_m.py -d socketcan --can-if can0
"""

import argparse
import sys
import time

import omnihand


def parse_options():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-d", choices=("zlgcan", "hcan", "socketcan"), default="zlgcan")
    parser.add_argument("-i", type=int, default=0, help="CAN adapter index")
    parser.add_argument("-c", type=int, default=0, help="CAN channel")
    parser.add_argument("--can-if", default="can0", help="SocketCAN interface")
    parser.add_argument("--device-id", type=int, default=0,
                        help="0 discovers the address and detects T1.3/T1.5")
    parser.add_argument("--hand", choices=("left", "right"), default="left")
    parser.add_argument("-f", type=int, help="Request interval in milliseconds")
    parser.add_argument("-t", type=int, help="Receive timeout in milliseconds")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print TX/RX details")
    options = parser.parse_args()
    if not 0 <= options.i <= 255 or not 0 <= options.c <= 255:
        parser.error("CAN adapter index and channel must be in [0, 255]")
    if not 0 <= options.device_id <= 127:
        parser.error("--device-id must be in [0, 127]")
    if options.f is not None and not 0 <= options.f <= 100:
        parser.error("-f must be in [0, 100]")
    if options.t is not None and options.t <= 0:
        parser.error("-t must be positive")
    return options


def create_hand(options):
    common = {
        "hand_type": (omnihand.HandType.LEFT if options.hand == "left"
                      else omnihand.HandType.RIGHT),
        "hand_device_id": options.device_id,
        "show_data_details": options.verbose,
    }
    if options.d == "socketcan":
        if not hasattr(omnihand.OmniHand3UltraM, "create_hand_socketcan"):
            raise RuntimeError("SocketCAN is unavailable on this platform")
        hand = omnihand.OmniHand3UltraM.create_hand_socketcan(
            **common, can_interface=options.can_if,
        )
    elif options.d == "hcan":
        hand = omnihand.OmniHand3UltraM.create_hand_by_hcan(
            **common, canfd_device_id=options.i, canfd_channel_id=options.c,
        )
    else:
        hand = omnihand.OmniHand3UltraM.create_hand_by_zlgcan(
            **common, canfd_device_id=options.i, canfd_channel_id=options.c,
        )
    # Factories initialize the connection; no separate init() is needed.
    if hand is None:
        raise RuntimeError(f"Failed to initialize OmniHand3UltraM via {options.d}")
    if options.f is not None:
        hand.set_request_interval(options.f)
    if options.t is not None:
        hand.set_frame_recv_timeout(options.t)
    return hand


def read_sensors(hand):
    print("Motor temperatures (C):", hand.get_all_temperature_reports())
    print("Motor currents (protocol units):", hand.get_all_current_reports())
    for index, error in enumerate(hand.get_all_error_reports(), start=1):
        print(f"Motor {index} error: {omnihand.h3um_error_report_to_string(error)}")

    pressure = hand.get_revision() == omnihand.H3UMRevision.T1P5
    if pressure:
        print(f"Palm board: {hand.get_temperature_of_palm()} C, "
              f"power board: {hand.get_temperature_of_power()} C")
    if not hand.init_tactile_points_map():
        print("Tactile metadata unavailable", file=sys.stderr)
        return

    # Read raw tactile data once. Some T1.5 firmware only replies to whole-hand
    # queries; T1.3 returns XYZ points, while T1.5 returns uint16 pressure values.
    sensors = hand.get_all_tactile_sensor_data_raw()
    if len(sensors) != len(hand.get_sensor_order()):
        print("Incomplete tactile data", file=sys.stderr)
        return
    for sensor in sensors:
        values = sensor.data if pressure else sensor.points
        if not values or len(values) != hand.get_num_of_tactile_points(sensor.sensor_id):
            print(f"{sensor.sensor_id.name}: incomplete tactile data", file=sys.stderr)
            continue
        if pressure:
            print(f"{sensor.sensor_id.name}: {len(values)} pressure points, "
                  f"first values: {values[:8]}")
        else:
            point = values[0]
            force = hand.get_tactile_sensor_resultant_force(sensor.sensor_id).force
            print(f"{sensor.sensor_id.name}: {len(values)} 3D points, "
                  f"first XYZ: {point.x} {point.y} {point.z}, "
                  f"resultant XYZ: {force.x} {force.y} {force.z}")


def run_demo(hand, name):
    print(f"{name} hand:\n{hand.get_vendor_info()}")
    # Preserve the other joints at their measured positions.
    angles = hand.get_all_active_joint_angles()
    if len(angles) != hand.get_doa():
        raise RuntimeError("Failed to read the initial joint angles")
    # Pinky PIP is index 2 (zero-based) in both revisions, in radians.
    for target in (0.35, 0.7):
        angles[2] = target
        feedback = hand.set_all_active_joint_angles(angles)
        if len(feedback) != len(angles):
            raise RuntimeError("Failed to send joint angles")
        time.sleep(1.0)
        actual = hand.get_all_active_joint_angles()
        if len(actual) != len(angles):
            raise RuntimeError("Failed to read joint angle feedback")
        print(f"pinky PIP target: {target} rad, actual: {actual[2]} rad")
    read_sensors(hand)


def main():
    options = parse_options()
    try:
        hand = create_hand(options)
        run_demo(hand, options.hand)
        return 0
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
