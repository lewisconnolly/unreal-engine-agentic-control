"""Tests for MCP server tools using FastMCP's in-memory client with mocked TCP."""

from __future__ import annotations

import json
from unittest.mock import AsyncMock, patch

import pytest
from fastmcp import Client

from mcp_server.server import mcp


def _mock_send_command(command: str, params: dict | None = None) -> dict:  # noqa: C901
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
    elif command == "delete_actor":
        actor_id = params.get("actor_id", "") if params else ""
        return {"success": True, "actor_id": actor_id}
    elif command == "set_transform":
        actor_id = params.get("actor_id", "") if params else ""
        # Echo back the params as the transform to verify they were passed through
        transform = {
            "location": {
                "x": params.get("x", 0.0),
                "y": params.get("y", 0.0),
                "z": params.get("z", 0.0),
            },
            "rotation": {
                "pitch": params.get("pitch", 0.0),
                "yaw": params.get("yaw", 0.0),
                "roll": params.get("roll", 0.0),
            },
            "scale": {
                "x": params.get("scale_x", 1.0),
                "y": params.get("scale_y", 1.0),
                "z": params.get("scale_z", 1.0),
            },
        }
        return {"success": True, "actor_id": actor_id, "transform": transform}
    elif command == "import_asset":
        file_path = params.get("file_path", "") if params else ""
        asset_name = params.get("asset_name", "") if params else ""
        return {
            "success": True,
            "asset_path": f"/Game/Generated/{asset_name}",
        }
    elif command == "apply_material":
        actor_id = params.get("actor_id", "") if params else ""
        return {
            "success": True,
            "actor_id": actor_id,
            "material_path": f"/Game/Generated/M_{actor_id}",
        }
    elif command == "set_visibility":
        actor_id = params.get("actor_id", "") if params else ""
        visible = params.get("visible", True) if params else True
        return {"success": True, "actor_id": actor_id, "visible": visible}
    elif command == "search_actors":
        query = (params.get("query", "") if params else "").lower()
        # Simulated scene for search tests
        scene = [
            {
                "actor_id": "Cube_1",
                "class": "StaticMeshActor",
                "transform": {
                    "location": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
            },
            {
                "actor_id": "PointLight_1",
                "class": "PointLight",
                "transform": {
                    "location": {"x": 100.0, "y": 0.0, "z": 200.0},
                    "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
            },
        ]
        results = [
            actor for actor in scene
            if query in actor["actor_id"].lower() or query in actor["class"].lower()
        ]
        return {"success": True, "query": params.get("query", ""), "results": results}
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


@pytest.mark.asyncio
async def test_delete_actor(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("delete_actor", {
            "actor_id": "Cube_1",
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "Cube_1"

    mock_tcp.assert_called_once_with("delete_actor", {"actor_id": "Cube_1"})


@pytest.mark.asyncio
async def test_set_transform_location_only(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("set_transform", {
            "actor_id": "Cube_1",
            "x": 500.0,
            "z": 200.0,
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "Cube_1"
        assert data["transform"]["location"]["x"] == 500.0
        assert data["transform"]["location"]["z"] == 200.0

    # Only non-None params should be sent (actor_id + x + z)
    mock_tcp.assert_called_once_with("set_transform", {
        "actor_id": "Cube_1",
        "x": 500.0,
        "z": 200.0,
    })


@pytest.mark.asyncio
async def test_set_transform_full(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("set_transform", {
            "actor_id": "Cube_1",
            "x": 100.0,
            "y": 200.0,
            "z": 300.0,
            "yaw": 45.0,
            "pitch": 10.0,
            "roll": 0.0,
            "scale_x": 2.0,
            "scale_y": 2.0,
            "scale_z": 2.0,
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "Cube_1"
        assert data["transform"]["location"]["x"] == 100.0
        assert data["transform"]["rotation"]["yaw"] == 45.0
        assert data["transform"]["scale"]["x"] == 2.0

    mock_tcp.assert_called_once_with("set_transform", {
        "actor_id": "Cube_1",
        "x": 100.0,
        "y": 200.0,
        "z": 300.0,
        "yaw": 45.0,
        "pitch": 10.0,
        "roll": 0.0,
        "scale_x": 2.0,
        "scale_y": 2.0,
        "scale_z": 2.0,
    })


@pytest.mark.asyncio
async def test_import_asset(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("import_asset", {
            "file_path": "/tmp/generated_images/brick_wall.png",
            "asset_name": "brick_wall",
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["asset_path"] == "/Game/Generated/brick_wall"

    mock_tcp.assert_called_once_with("import_asset", {
        "file_path": "/tmp/generated_images/brick_wall.png",
        "asset_name": "brick_wall",
    })


@pytest.mark.asyncio
async def test_apply_material(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("apply_material", {
            "actor_id": "Floor_1",
            "texture_asset_path": "/Game/Generated/brick_wall",
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "Floor_1"
        assert data["material_path"] == "/Game/Generated/M_Floor_1"

    mock_tcp.assert_called_once_with("apply_material", {
        "actor_id": "Floor_1",
        "texture_asset_path": "/Game/Generated/brick_wall",
    })


@pytest.mark.asyncio
async def test_search_actors_by_label(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("search_actors", {"query": "Cube"})
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["query"] == "Cube"
        assert len(data["results"]) == 1
        assert data["results"][0]["actor_id"] == "Cube_1"

    mock_tcp.assert_called_once_with("search_actors", {"query": "Cube"})


@pytest.mark.asyncio
async def test_search_actors_by_class(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("search_actors", {"query": "PointLight"})
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert len(data["results"]) == 1
        assert data["results"][0]["actor_id"] == "PointLight_1"
        assert data["results"][0]["class"] == "PointLight"

    mock_tcp.assert_called_once_with("search_actors", {"query": "PointLight"})


@pytest.mark.asyncio
async def test_search_actors_no_match(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("search_actors", {"query": "NonExistent"})
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["results"] == []

    mock_tcp.assert_called_once_with("search_actors", {"query": "NonExistent"})


@pytest.mark.asyncio
async def test_set_visibility_true(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("set_visibility", {
            "actor_id": "Cube_1",
            "visible": True,
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "Cube_1"
        assert data["visible"] is True

    mock_tcp.assert_called_once_with("set_visibility", {
        "actor_id": "Cube_1",
        "visible": True,
    })


@pytest.mark.asyncio
async def test_set_visibility_false(mock_tcp):
    async with Client(mcp) as client:
        result = await client.call_tool("set_visibility", {
            "actor_id": "PointLight_1",
            "visible": False,
        })
        data = json.loads(result.content[0].text)
        assert data["success"] is True
        assert data["actor_id"] == "PointLight_1"
        assert data["visible"] is False

    mock_tcp.assert_called_once_with("set_visibility", {
        "actor_id": "PointLight_1",
        "visible": False,
    })
