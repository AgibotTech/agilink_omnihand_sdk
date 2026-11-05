"""Command-line entry point for the native OmniHand server."""

import argparse
from pathlib import Path

from . import OmniHandServer, ServerOptions


def main() -> None:
    parser = argparse.ArgumentParser(description="OmniHand REST/WebSocket server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--publish-interval-ms", type=int, default=100)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--static-file-root", default="")
    args = parser.parse_args()

    options = ServerOptions()
    options.host = args.host
    options.port = args.port
    options.publish_interval_ms = args.publish_interval_ms
    options.log_level = args.log_level
    options.static_file_root = args.static_file_root or str(Path(__file__).resolve().parent)

    server = OmniHandServer(options)
    server.start()
    print(f"OmniHand server listening on http://{options.host}:{server.port}")
    try:
        server.wait()
    except KeyboardInterrupt:
        server.stop()


if __name__ == "__main__":
    main()
