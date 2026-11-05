"""WebSocket client demo for OmniHand Server.

Install WebSocket support with ``pip install omnihand[client]`` first.
"""

import argparse
import asyncio
import json
import sys

from omnihand_client import OmniHandClientError, OmniHandWebSocketClient


def build_conn_config(device: str, uart_port: str = "") -> dict:
    if device == "socketcan":
        return {"can_interface": "can0"}
    if device in ("rs485", "usb"):
        if not uart_port:
            raise ValueError(f"--uart-port is required for {device}")
        return {"uart_port": uart_port}
    return {"canfd_device_id": 0, "canfd_channel_id": 0}


async def run(host: str, port: int, device: str, uart_port: str = "") -> None:
    async with OmniHandWebSocketClient(host, port) as client:
        print(f"[list] -> {await client.list_hands()}")

        methods = await client.methods(product_type="omnihand_2025")
        print(f"[methods] -> {json.dumps(methods, indent=2)}")

        hand = await client.create_hand(
            "omnihand_2025",
            device,
            build_conn_config(device, uart_port),
        )
        hand_id = hand["hand_id"]
        try:
            print(f"[create] -> {json.dumps(hand, indent=2)}")
            print(f"[get_vendor_info] -> {await client.call(hand_id, 'get_vendor_info')}")
            print(f"[get_joint_names] -> {await client.call(hand_id, 'get_joint_names')}")
            positions = await client.call(hand_id, "get_all_joint_positions")
            print(f"[get_all_joint_positions] -> {positions}")
        finally:
            print(f"[remove] -> {await client.remove_hand(hand_id)}")


def main() -> None:
    parser = argparse.ArgumentParser(description="OmniHand WebSocket client demo")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument(
        "-d",
        "--device",
        default="zlgcan",
        choices=["zlgcan", "hcan", "socketcan", "rs485", "usb"],
    )
    parser.add_argument("--uart-port", default="")
    args = parser.parse_args()

    try:
        asyncio.run(run(args.host, args.port, args.device, args.uart_port))
    except (ImportError, OmniHandClientError, ValueError) as exc:
        print(f"[Error] {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
