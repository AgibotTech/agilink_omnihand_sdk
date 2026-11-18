# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

"""Sweep O10 active joints 5, 6, 8, 10 (1-based) from min to max once.

Usage:
  python demo_joint_rad_sweep.py --hand left --rad-step 0.001 --hz 50 -o sweep.csv
  python demo_joint_rad_sweep.py --hand right --rad-step 0.001 --hz 50 -d hcan
  python demo_joint_rad_sweep.py --hand right -d rs485 --uart-port /dev/ttyACM0

The four finger PIP joints move together from 0 to 1.48 rad on either hand.
Other motors retain their positions read at startup. The first command moves
the selected joints to min, followed by a one-second settling pause. Subsequent
commands increment the angle by rad-step at the requested frequency, including
the exact max endpoint. Bus latency and CSV writing may reduce actual Hz.

Each command writes one CSV row with sample number, elapsed_ms (command send
time), request_ms, status, and four arrays expanded into columns for joints
1..10: input_rad_N, input_encoder_N, returned_encoder_N, returned_rad_N.
returned_encoder is the reply from set_all_joint_positions(); returned_rad is
converted from that same reply by the hand-specific SDK solver, without a
separate get request. Missing reply values are blank and stop the sweep.
CSV output is buffered without per-row flushes or terminal output. Normal
completion, Ctrl+C, and exceptions close the file and flush buffered rows.
"""

import argparse
import csv
import math
import sys
import time

from omnihand import HandType, OmniHand2025, OmniHand2025Solver


NUM_JOINTS = 10
SWEEP_JOINT_IDS = (5, 6, 8, 10)
SWEEP_INDICES = tuple(joint_id - 1 for joint_id in SWEEP_JOINT_IDS)
# kActiveJointMin/Max in cpp/src/omnihand/kinematics/omnihand_2025/
# omnihand_2025_solver.h. These four limits are identical for left/right hands.
MIN_RAD = 0.0
MAX_RAD = 1.48
CSV_HEADER = ['sample', 'elapsed_ms', 'request_ms', 'status'] + [
    f'{field}_{joint_id}'
    for field in ('input_rad', 'input_encoder', 'returned_encoder', 'returned_rad')
    for joint_id in range(1, NUM_JOINTS + 1)
]


def positive_float(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be a finite number greater than 0")
    return number


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--hand', choices=['left', 'right'], default='left')
    parser.add_argument('--rad-step', type=positive_float, default=0.001,
                        help='Angle increment per command in rad (default: 0.001)')
    parser.add_argument('--hz', type=positive_float, default=50.0,
                        help='Target command frequency in Hz (default: 50)')
    parser.add_argument('-o', '--output', default='joint_rad_sweep.csv',
                        help='Output CSV path (default: joint_rad_sweep.csv)')
    parser.add_argument('-d', '--device', default='zlgcan',
                        choices=['zlgcan', 'hcan', 'rs485', 'zlgcan_tcp', 'usb', 'socketcan'])
    parser.add_argument('--hand-id', type=int, default=OmniHand2025.kDefaultHandDeviceId)
    parser.add_argument('--canfd-device-id', type=int, default=0)
    parser.add_argument('--canfd-channel-id', type=int, default=0)
    parser.add_argument('--uart-port', default='/dev/ttyACM0')
    parser.add_argument('--can-interface', default='can0')
    parser.add_argument('--host', default='192.168.0.178')
    parser.add_argument('--port', type=int, default=8000)
    parser.add_argument('--timeout-ms', type=int, default=100,
                        help='Reply timeout, 10..1000 ms (default: 100)')
    args = parser.parse_args()
    if not 10 <= args.timeout_ms <= 1000:
        parser.error('--timeout-ms must be between 10 and 1000')
    return args


def create_hand(args):
    common = dict(hand_type=HandType.LEFT if args.hand == 'left' else HandType.RIGHT,
                  hand_device_id=args.hand_id)
    if args.device == 'hcan':
        return OmniHand2025.create_hand_by_hcan(
            **common, canfd_device_id=args.canfd_device_id,
            canfd_channel_id=args.canfd_channel_id)
    if args.device == 'rs485':
        return OmniHand2025.create_hand_by_rs485(**common, uart_port=args.uart_port)
    if args.device == 'usb':
        return OmniHand2025.create_hand_by_usb(**common, uart_port=args.uart_port)
    if args.device == 'zlgcan_tcp':
        return OmniHand2025.create_hand_by_zlgcan_tcp(
            **common, host=args.host, port=args.port,
            canfd_channel_id=args.canfd_channel_id)
    if args.device == 'socketcan':
        return OmniHand2025.create_hand_socketcan(**common, can_interface=args.can_interface)
    return OmniHand2025.create_hand_by_zlgcan(
        **common, canfd_device_id=args.canfd_device_id,
        canfd_channel_id=args.canfd_channel_id)


def sweep_values(rad_step):
    """Yield min, fixed increments, and the exact max without overshoot."""
    step = 0
    while MIN_RAD + step * rad_step < MAX_RAD:
        yield MIN_RAD + step * rad_step
        step += 1
    yield MAX_RAD


def run_sweep(hand, solver, rad_step, hz, writer):
    initial_encoder = hand.get_all_joint_positions()
    if initial_encoder is None or len(initial_encoder) != NUM_JOINTS:
        raise RuntimeError(f'Expected 10 initial motor positions, got {initial_encoder!r}')
    initial_encoder = list(initial_encoder)
    initial_rad = solver.actuator_input_to_active_joint_pos(initial_encoder)
    if len(initial_rad) != NUM_JOINTS:
        raise RuntimeError('Failed to convert initial motor positions to radians')

    period = 1.0 / hz
    start_time = next_tick = time.perf_counter()
    sample_count = 0
    for sample, angle in enumerate(sweep_values(rad_step)):
        delay = next_tick - time.perf_counter()
        if delay > 0:
            time.sleep(delay)

        input_rad = list(initial_rad)
        for index in SWEEP_INDICES:
            input_rad[index] = angle
        converted = solver.active_joint_pos_to_actuator_input(input_rad)
        if len(converted) != NUM_JOINTS:
            raise RuntimeError('Failed to convert input radians to motor positions')
        # Keep other motors exact: avoid moving them due to solver round-trip error.
        input_encoder = list(initial_encoder)
        for index in SWEEP_INDICES:
            input_encoder[index] = converted[index]

        returned_encoder = []
        returned_rad = []
        request_ms = None
        status = 'interrupted'
        sent_at = time.perf_counter()
        try:
            reply = hand.set_all_joint_positions(input_encoder)
            request_ms = (time.perf_counter() - sent_at) * 1000.0
            if reply is not None:
                returned_encoder = list(reply)
            if len(returned_encoder) != NUM_JOINTS:
                raise RuntimeError('Expected 10 motor positions in set reply')
            returned_rad = list(solver.actuator_input_to_active_joint_pos(returned_encoder))
            if len(returned_rad) != NUM_JOINTS:
                raise RuntimeError('Failed to convert returned motor positions to radians')
        except Exception as exc:
            status = f'error: {exc}'
            raise
        else:
            status = 'ok'
        finally:
            # Retain the failed sample too, padding missing feedback with blanks.
            # The file's buffer handles writes; do not flush each control cycle.
            if request_ms is None:
                request_ms = (time.perf_counter() - sent_at) * 1000.0
            writer.writerow(
                [sample, (sent_at - start_time) * 1000.0, request_ms, status]
                + input_rad + input_encoder
                + (returned_encoder + [''] * NUM_JOINTS)[:NUM_JOINTS]
                + (returned_rad + [''] * NUM_JOINTS)[:NUM_JOINTS]
            )
        sample_count += 1

        if sample == 0:
            time.sleep(1.0)
            next_tick = time.perf_counter()
        else:
            # If a reply/write overruns the period, avoid a burst of catch-up commands.
            next_tick = max(next_tick + period, time.perf_counter())
    return sample_count


def main():
    args = parse_args()
    try:
        # Open before any motion; the context manager flushes even on Ctrl+C/errors.
        with open(args.output, 'w', newline='', encoding='utf-8', buffering=1024 * 1024) as f:
            writer = csv.writer(f)
            writer.writerow(CSV_HEADER)
            hand = create_hand(args)
            if hand is None or not hand.init():
                raise RuntimeError('Failed to initialize hand')
            hand.set_request_interval(0)
            hand.set_frame_recv_timeout(args.timeout_ms)
            solver = OmniHand2025Solver(args.hand == 'left')
            print(f'hand={args.hand}, device={args.device}, joint IDs={SWEEP_JOINT_IDS} (1-based)')
            print(f'range={MIN_RAD}..{MAX_RAD} rad, step={args.rad_step} rad, target={args.hz} Hz')
            print(f'CSV output: {args.output}')
            sample_count = run_sweep(hand, solver, args.rad_step, args.hz, writer)
        print(f'Sweep complete: {sample_count} samples saved to {args.output}')
        return 0
    except KeyboardInterrupt:
        print('\nInterrupted; no further commands will be sent.')
        return 130
    except Exception as exc:
        print(f'Error: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
