"""REST client demo for OmniHand Server.

Start ``omnihand_server`` or ``python -m omnihand_server`` first, then run::

    python demo_rest_client.py --host 127.0.0.1 --port 8000 -d zlgcan
"""

import argparse
import json
import sys

from omnihand_client import OmniHandClientError, OmniHandRestClient


def build_conn_config(device: str, uart_port: str = "") -> dict:
    if device == "socketcan":
        return {"can_interface": "can0"}
    if device in ("rs485", "usb"):
        if not uart_port:
            raise ValueError(f"--uart-port is required for {device}")
        return {"uart_port": uart_port}
    return {"canfd_device_id": 0, "canfd_channel_id": 0}


def run(host: str, port: int, device: str, uart_port: str = "") -> None:
    client = OmniHandRestClient(host, port)
    print(f"[health] -> {client.health()}")

    methods = client.methods(product_type="omnihand_2025")
    method_names = [method["name"] for method in methods["methods"]]
    print(f"[methods] ({len(method_names)}): {method_names}")
    print(f"[hands] -> {client.list_hands()}")

    hand = client.create_hand(
        "omnihand_2025",
        device,
        build_conn_config(device, uart_port),
    )
    try:
        print(f"[create] -> {json.dumps(hand.summary, indent=2)}")
        print(f"[describe] -> {json.dumps(hand.describe(), indent=2)}")
        print(f"[get_vendor_info] -> {hand.call('get_vendor_info')}")
        print(f"[get_joint_names] -> {hand.call('get_joint_names')}")
        print(f"[get_all_joint_positions] -> {hand.call('get_all_joint_positions')}")
    finally:
        print(f"[remove] -> {hand.close()}")


def main() -> None:
    parser = argparse.ArgumentParser(description="OmniHand REST client demo")
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
        run(args.host, args.port, args.device, args.uart_port)
    except (OmniHandClientError, ValueError) as exc:
        print(f"[Error] {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
