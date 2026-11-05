"""Black-box tests for the public OmniHand Server client APIs."""

import asyncio

import pytest

from omnihand_client import (
    OmniHandRestClient,
    OmniHandClientError,
    OmniHandConnectionError,
)


def _instructions() -> str:
    return (
        "Start either the C++ or Python OmniHand Server before running this test:\n"
        "  C++:    omnihand_server\n"
        "  Python: python -m omnihand_server"
    )


@pytest.fixture(scope="module")
def server_client(request: pytest.FixtureRequest) -> OmniHandRestClient:
    instructions = _instructions()
    terminal = request.config.pluginmanager.get_plugin("terminalreporter")
    if terminal is not None:
        terminal.write_line(instructions)
    else:
        print(instructions)

    client = OmniHandRestClient(timeout=3)
    try:
        client.health()
    except OmniHandConnectionError as error:
        pytest.fail(f"{error}\n{instructions}", pytrace=False)
    return client


def test_rest_client_executes_discovery_tasks(server_client: OmniHandRestClient) -> None:
    health = server_client.health()
    assert health["status"] == "ok"
    assert isinstance(health["hands"], int)
    assert isinstance(server_client.list_hands(), list)
    assert "omnihand_2025" in server_client.list_product_types()

    methods = server_client.methods(product_type="omnihand_2025")
    method_names = {method["name"] for method in methods["methods"]}
    assert "get_all_joint_positions" in method_names
    assert "set_all_joint_positions" in method_names


def test_rest_client_reports_protocol_errors(server_client: OmniHandRestClient) -> None:
    with pytest.raises(OmniHandClientError) as caught:
        server_client.create_hand("not_a_product", "hcan")

    assert caught.value.status_code == 422
    assert caught.value.detail == "Invalid hand_type: not_a_product"


def test_websocket_client_executes_discovery_tasks(server_client: OmniHandRestClient) -> None:
    pytest.importorskip("websockets", reason="install omnihand[client] to test WebSocket support")

    async def run() -> None:
        async with server_client.websocket() as websocket:
            assert isinstance(await websocket.list_hands(), list)
            methods = await websocket.methods(product_type="omnihand_2025")
            assert any(
                method["name"] == "get_all_joint_positions"
                for method in methods["methods"]
            )

    asyncio.run(run())
