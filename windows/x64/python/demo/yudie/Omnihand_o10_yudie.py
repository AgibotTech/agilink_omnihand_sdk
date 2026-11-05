# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2.

from omnihand import OmniHand2025, Finger, ControlMode, HandType
from enum import Enum

def init_hand(hand_type: str = "right", device_type: str = "zlgcan"):
    """
    Initialize hand
    """
    if device_type == "hcan":
        create_hand = OmniHand2025.create_hand_by_hcan
    else:
        create_hand = OmniHand2025.create_hand_by_zlgcan

    if hand_type.lower() == "right":
        hand = create_hand(hand_device_id=OmniHand2025.kDefaultHandDeviceId, canfd_device_id=0, hand_type=HandType.RIGHT, canfd_channel_id=0)
        return hand
    elif hand_type.lower() == "left":
        hand = create_hand(hand_device_id=OmniHand2025.kDefaultHandDeviceId, canfd_device_id=0, hand_type=HandType.LEFT, canfd_channel_id=0)
        return hand
    else:
        left_hand = create_hand(hand_device_id=OmniHand2025.kDefaultHandDeviceId, canfd_device_id=0, hand_type=HandType.LEFT, canfd_channel_id=0)
        right_hand = create_hand(hand_device_id=OmniHand2025.kDefaultHandDeviceId, canfd_device_id=0, hand_type=HandType.RIGHT, canfd_channel_id=1)
        # Verbose log for CAN traffic
        left_hand.show_data_details(True)
        right_hand.show_data_details(True)
        return left_hand, right_hand

def set_hand_position(
    hand: OmniHand2025,
    positions: list,
    glove_type: str | None = None,
    hand_type: str | None = None,
):
    """
    Set hand positions. Cross-hand control is converted by the SDK.
    """
    target_positions = positions
    if glove_type is not None and hand_type is not None and glove_type != hand_type:
        source = HandType.LEFT if glove_type == "left" else HandType.RIGHT
        target = HandType.LEFT if hand_type == "left" else HandType.RIGHT
        target_positions = hand.convert_active_joint_angles(
            positions, source, target
        )
        if not target_positions:
            raise ValueError("failed to convert O10 active-joint angles")

    hand.set_all_active_joint_angles(target_positions)
    print("get active joint angles:", hand.get_all_active_joint_angles())
