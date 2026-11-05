"""Thin lifecycle facade for the native OmniHand Server."""

from typing import Any

__all__ = ["OmniHandServer", "ServerOptions"]


def __getattr__(name: str) -> Any:
    if name in __all__:
        from omnihand import OmniHandServer, ServerOptions

        return {"OmniHandServer": OmniHandServer, "ServerOptions": ServerOptions}[name]
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
