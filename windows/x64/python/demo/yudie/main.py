import argparse
from glove_receiver import GloveReceiver
import time

def main():
    parser = argparse.ArgumentParser(description='Dexterous hand control main program')
    parser.add_argument('-d', '--device', choices=['zlgcan', 'hcan'], default='hcan',
                        help='CAN backend: zlgcan (ZLG USB CANFD) or hcan (HCAN USB CANFD); default zlgcan')
    parser.add_argument('--mode', choices=['o10', 'o10_hal'], default='o10', help='Hand stack selection')
    parser.add_argument('--ip', help='UDP bind IP (default 127.0.0.1)', default='127.0.0.1')
    parser.add_argument('--port', type=int, help='UDP port (default 7777)', default=7777)
    parser.add_argument('--hal_ip', help='HAL service IP (default 127.0.0.1)', default='127.0.0.1')
    parser.add_argument('--hal_port', type=int, help='HAL service port (default 56421)', default=56421)
    parser.add_argument('--glove', choices=['left', 'right', 'both'],
                        help='Glove side. If omitted, follows --hand')
    parser.add_argument('--hand', choices=['left', 'right', 'both'],
                        help='O10 side. If omitted, follows --glove')
    parser.add_argument('--user_name', type=str, help='HandDrive username (default teleop)', default='teleop')
    args, unknown = parser.parse_known_args()

    if args.glove is None and args.hand is None:
        args.glove = args.hand = 'both'
    elif args.glove is None:
        args.glove = args.hand
    elif args.hand is None:
        args.hand = args.glove

    if (args.glove == 'both') != (args.hand == 'both'):
        parser.error("--glove and --hand must both be 'both', or both select one side")

    # unified UDP receive
    sdk = GloveReceiver()
    sdk.server_addr = (args.ip, args.port)
    sdk.initialize()
    sdk.start_listening()

    # Lingxin dexterous-hand branch
    if args.mode == 'o10':
        from Omnihand_o10_yudie import init_hand, set_hand_position
        if args.hand == 'both':
            left_hand, right_hand = init_hand(args.hand, args.device)
        else:
            hand = init_hand(args.hand, args.device)
        def update():
            role_list = sdk.get_role_name_list()
            if len(role_list) > 0:
                if args.hand == 'both':
                    left_joints = sdk.get_finger_data_for_o10hand(args.user_name, 'left')
                    right_joints = sdk.get_finger_data_for_o10hand(args.user_name, 'right')
                    print(f"Finger Data: {left_joints} | {right_joints}")
                    set_hand_position(left_hand, left_joints, 'left', 'left')
                    set_hand_position(right_hand, right_joints, 'right', 'right')
                else:
                    joints = sdk.get_finger_data_for_o10hand(args.user_name, args.glove)
                    print(f"Finger Data: {joints}")
                    set_hand_position(hand, joints, args.glove, args.hand)

    elif args.mode == 'o10_hal':
        # New path: HAL service (solver + curl)
        # Use main.py ip/port as HAL_HOST/HAL_PORT
        from o10_hand_service import set_hand_position
        def update():
            role_list = sdk.get_role_name_list()
            if len(role_list) > 0:
                if args.hand == 'both':
                    left_joints = sdk.get_finger_data_for_o10hand(args.user_name, 'left')
                    right_joints = sdk.get_finger_data_for_o10hand(args.user_name, 'right')
                    print(f"Finger Data: {left_joints} | {right_joints}")
                    set_hand_position('left', left_joints, args.hal_ip, args.hal_port,
                                      glove_type='left')
                    set_hand_position('right', right_joints, args.hal_ip, args.hal_port,
                                      glove_type='right')
                else:
                    joints = sdk.get_finger_data_for_o10hand(args.user_name, args.glove)
                    print(f"Finger Data: {joints}")
                    set_hand_position(args.hand, joints, args.hal_ip, args.hal_port,
                                      glove_type=args.glove)

    try:
        while True:
            update()
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\nInterrupted, exiting...")
    finally:
        sdk.end_listening()


if __name__ == "__main__":
    main()
