"""Tests for MCP server tools using FastMCP's in-memory client with mocked TCP."""

from __future__ import annotations

import json
from unittest.mock import AsyncMock, patch

import pytest
from fastmcp import Client

from mcp_server.server import mcp


def _mock_send_command(command: str, params: dict | None = None) -> dict:
    """Return stub responses matching what the UE plugin would send."""
    if command == "spawn_actor":
        actor_type = params.get("actor_type", "Unknown") if params else "Unknown"
        return {
            "success": True,
            "actor_id": f"{actor_type}_1",
            "actor_type": actor_type,
            "position": {
                "x": params.get("x", 0) if params else 0,
                "y": params.get("y", 0) if params else 0,
                "z": params.get("z", 0) if params else 0,
            },
        }
    elif command == "get_scene_info":
        return {"success": True, "actors": []}
    return {"success": False, "error": "Unknown command"}


@pytest.fixture
def mock_tcp():
    with patch("mcp_server.server.send_command", new_callable=AsyncMock) as mock:
        mock.side_effect = _mock_send_command
        yield mock


@pytest.mark.asyncio
async def test_spawn_actor(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("spawn_actor", {
            "actor_type": "StaticMeshActor",
            "x": 100.0,
            "y": 200.0,
            "z": 300.0,
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "StaticMeshActor_1"
        assert data["position"]["x"] == 100.0
        assert data["position"]["y"] == 200.0
        assert data["position"]["z"] == 300.0

    mock_tcp.assert_called_once_with("spawn_actor", {
        "actor_type": "StaticMeshActor",
        "x": 100.0,
        "y": 200.0,
        "z": 300.0,
    })


@pytest.mark.asyncio
async def test_get_scene_info(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("get_scene_info", {})
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actors"] == []

    mock_tcp.assert_called_once_with("get_scene_info")


@pytest.mark.asyncio
async def test_spawn_actor_different_types(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("spawn_actor", {
            "actor_type": "PointLight",
            "x": 0.0,
            "y": 0.0,
            "z": 500.0,
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "PointLight_1"
        assert data["actor_type"] == "PointLight"
