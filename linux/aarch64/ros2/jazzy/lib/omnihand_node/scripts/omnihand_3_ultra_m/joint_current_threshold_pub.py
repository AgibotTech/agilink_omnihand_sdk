#!/usr/bin/env python3
"""
@Description: Publish current threshold values to OmniHand3UltraM T1.3/T1.5 and
              receive the read-back on joint_current_threshold_states.

Topic:
  pub: /<product>/<side>/joint_current_threshold_cmd  (std_msgs/Int16MultiArray)
  sub: /<product>/<side>/joint_current_threshold_states  (std_msgs/Int16MultiArray)

Usage:  python3 joint_current_threshold_pub.py <threshold> [left|right] [product]
        default: threshold=500, side=left, product=h3u_m
"""

import os
import sys

_scripts = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _scripts not in sys.path:
    sys.path.insert(0, _scripts)

import rclpy
from rclpy.node import Node
from std_msgs.msg import Int16MultiArray
from omnihand_node_msgs.msg import Int16MultiArrayStamped
from joint_info import get_joint_names, wait_for_subscription
from ros_multi_array_utils import make_int16_multi_array

# topic:
# /h3u_m/left/joint_current_threshold_cmd; /h3u_m/right/joint_current_threshold_cmd;
# /h3u_m/left/joint_current_threshold_states; /h3u_m/right/joint_current_threshold_states


class CurrentThresholdPubSub(Node):
    def __init__(self, hand_side: str, product: str):
        super().__init__(f'{product}_{hand_side}_current_threshold_pub')
        self.product = product
        self.hand_side = hand_side
        self.command_sent = False
        self.publisher = self.create_publisher(
            Int16MultiArray, f'/{product}/{hand_side}/joint_current_threshold_cmd', 10)
        self.subscription = self.create_subscription(
            Int16MultiArrayStamped, f'/{product}/{hand_side}/joint_current_threshold_states', self.callback, 10)

    def start(self, threshold):
        num_joints = len(get_joint_names(self, self.product, self.hand_side))
        wait_for_subscription(self, self.publisher)
        self.command_sent = True
        self.publisher.publish(make_int16_multi_array([threshold] * num_joints))
        self.get_logger().info(
            f'{self.product}/{self.hand_side} set current_threshold={threshold} for all '
            f'{num_joints} joints')

    def callback(self, msg: Int16MultiArrayStamped):
        if not self.command_sent:
            return
        pairs = [f'joint_{i}={msg.data[i]}' for i in range(len(msg.data))]
        self.get_logger().info(f'{self.product}/{self.hand_side} current_threshold read-back: [{", ".join(pairs)}]')
        raise SystemExit(0)


def main(args=None):
    rclpy.init(args=args)
    threshold = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    hand_side = sys.argv[2].lower() if len(sys.argv) > 2 else 'left'
    product = sys.argv[3].lower() if len(sys.argv) > 3 else 'h3u_m'
    node = CurrentThresholdPubSub(hand_side, product)
    try:
        node.start(threshold)
        rclpy.spin(node)
    except SystemExit:
        pass
    except RuntimeError as error:
        node.get_logger().error(str(error))
        raise SystemExit(1)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        try:
            rclpy.shutdown()
        except Exception:
            pass


if __name__ == '__main__':
    main()
