"""Read detected H3UM joint metadata without sending hardware commands."""

import time

import rclpy
from rcl_interfaces.msg import ParameterType
from rcl_interfaces.srv import GetParameters


def get_joint_names(node, product, side, timeout_sec=5.0):
    topic = f'/{product}/{side}/joint_states'
    deadline = time.monotonic() + timeout_sec
    endpoints = []
    while rclpy.ok() and time.monotonic() < deadline:
        endpoints = node.get_publishers_info_by_topic(topic)
        if endpoints:
            break
        rclpy.spin_once(node, timeout_sec=min(0.05, max(
            0.0, deadline - time.monotonic())))
    if len(endpoints) != 1:
        raise RuntimeError(f'Expected one publisher for {topic}; found {len(endpoints)}')
    endpoint = endpoints[0]
    owner = f'{endpoint.node_namespace.rstrip("/")}/{endpoint.node_name}'
    client = node.create_client(GetParameters, f'{owner}/get_parameters')
    try:
        remaining = max(0.0, deadline - time.monotonic())
        if not client.wait_for_service(timeout_sec=remaining):
            raise RuntimeError(f'Timed out waiting for {owner}/get_parameters')
        request = GetParameters.Request()
        request.names = ['joint_names']
        future = client.call_async(request)
        rclpy.spin_until_future_complete(
            node, future, timeout_sec=max(0.0, deadline - time.monotonic()))
        if not future.done():
            future.cancel()
            raise RuntimeError(f'Timed out reading joint_names from {owner}')
        response = future.result()
        if (response is None or len(response.values) != 1 or
                response.values[0].type != ParameterType.PARAMETER_STRING_ARRAY):
            raise RuntimeError(f'{owner} does not provide joint_names as a string array')
        names = list(response.values[0].string_array_value)
        if (len(names) not in (20, 21) or any(not name for name in names) or
                len(set(names)) != len(names)):
            raise RuntimeError(f'{owner} returned invalid H3UM joint_names')
        return names
    finally:
        node.destroy_client(client)


def wait_for_subscription(node, publisher, timeout_sec=5.0):
    """Wait for DDS discovery before publishing a one-shot command."""
    deadline = time.monotonic() + timeout_sec
    while rclpy.ok() and time.monotonic() < deadline:
        if publisher.get_subscription_count() > 0:
            return
        rclpy.spin_once(node, timeout_sec=min(0.05, max(
            0.0, deadline - time.monotonic())))
    raise RuntimeError(f'No subscriber for {publisher.topic_name}')
