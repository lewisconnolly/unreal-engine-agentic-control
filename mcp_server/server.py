"""FastMCP server exposing Unreal Engine control tools over stdio.

Tools communicate with the UE TCP plugin via newline-delimited JSON over TCP.
"""

from __future__ import annotations

import asyncio
import json
import os

from dotenv import load_dotenv
from fastmcp import FastMCP

load_dotenv()

UE_TCP_HOST = os.getenv("UE_TCP_HOST", "127.0.0.1")
UE_TCP_PORT = int(os.getenv("UE_TCP_PORT", "9000"))

mcp = FastMCP("UnrealEngineControl")


async def send_command(command: str, params: dict | None = None) -> dict:
    """Send a JSON command to the UE TCP plugin and return the parsed response."""
    message = {"command": command}
    if params:
        message["params"] = params

    reader, writer = await asyncio.open_connection(UE_TCP_HOST, UE_TCP_PORT)
    try:
        payload = json.dumps(message) + "\n"
        writer.write(payload.encode())
        await writer.drain()

        data = await reader.readline()
        return json.loads(data.decode())
    finally:
        writer.close()
        await writer.wait_closed()


@mcp.tool
async def spawn_actor(actor_type: str, x: float, y: float, z: float) -> str:
    """Spawn a new actor in the Unreal Engine scene.

    Args:
        actor_type: The type of actor to spawn (e.g. 'StaticMeshActor', 'PointLight').
        x: X position in world space.
        y: Y position in world space.
        z: Z position in world space.

    Returns:
        JSON string with spawn result including the new actor's ID.
    """
    result = await send_command("spawn_actor", {
        "actor_type": actor_type,
        "x": x,
        "y": y,
        "z": z,
    })
    return json.dumps(result)


@mcp.tool
async def get_scene_info() -> str:
    """Query the current Unreal Engine scene for all actors and their properties.

    Returns:
        JSON string with scene information including actor list.
    """
    result = await send_command("get_scene_info")
    return json.dumps(result)


@mcp.tool
async def delete_actor(actor_id: str) -> str:
    """Delete an actor from the Unreal Engine scene.

    Args:
        actor_id: The ID (label) of the actor to delete.

    Returns:
        JSON string with the deletion result.
    """
    result = await send_command("delete_actor", {"actor_id": actor_id})
    return json.dumps(result)


@mcp.tool
async def set_transform(
    actor_id: str,
    x: float | None = None,
    y: float | None = None,
    z: float | None = None,
    yaw: float | None = None,
    pitch: float | None = None,
    roll: float | None = None,
    scale_x: float | None = None,
    scale_y: float | None = None,
    scale_z: float | None = None,
) -> str:
    """Set the transform (position, rotation, scale) of an existing actor.

    Only provided parameters are updated; omitted parameters keep their current values.

    Args:
        actor_id: The ID (label) of the actor to transform.
        x: X position in world space.
        y: Y position in world space.
        z: Z position in world space.
        yaw: Yaw rotation in degrees.
        pitch: Pitch rotation in degrees.
        roll: Roll rotation in degrees.
        scale_x: X scale factor.
        scale_y: Y scale factor.
        scale_z: Z scale factor.

    Returns:
        JSON string with the updated transform.
    """
    params: dict = {"actor_id": actor_id}
    for key, value in [
        ("x", x), ("y", y), ("z", z),
        ("yaw", yaw), ("pitch", pitch), ("roll", roll),
        ("scale_x", scale_x), ("scale_y", scale_y), ("scale_z", scale_z),
    ]:
        if value is not None:
            params[key] = value

    result = await send_command("set_transform", params)
    return json.dumps(result)


@mcp.tool
async def import_asset(file_path: str, asset_name: str) -> str:
    """Import an external file (e.g. a generated image) into the UE project as an asset.

    Args:
        file_path: Absolute path to the file on disk to import.
        asset_name: Name for the imported asset inside UE (placed under /Game/Generated/).

    Returns:
        JSON string with import result including the UE asset path.
    """
    result = await send_command("import_asset", {
        "file_path": file_path,
        "asset_name": asset_name,
    })
    return json.dumps(result)


@mcp.tool
async def apply_material(actor_id: str, texture_asset_path: str) -> str:
    """Apply a texture as a material to an actor's mesh.

    Args:
        actor_id: The ID (label) of the actor to apply the material to.
        texture_asset_path: The UE asset path of the texture (e.g. /Game/Generated/my_texture).

    Returns:
        JSON string with the result including the created material path.
    """
    result = await send_command("apply_material", {
        "actor_id": actor_id,
        "texture_asset_path": texture_asset_path,
    })
    return json.dumps(result)


@mcp.tool
async def search_actors(query: str) -> str:
    """Search for actors in the scene by name or class.

    Performs a case-insensitive substring match against actor labels and class names.
    Use this to resolve ambiguous references (e.g. "the cube", "a light") to exact actor IDs.

    Args:
        query: Search string to match against actor labels and class names.

    Returns:
        JSON string with matching actors including their IDs, classes, and transforms.
    """
    result = await send_command("search_actors", {"query": query})
    return json.dumps(result)


@mcp.tool
async def set_visibility(actor_id: str, visible: bool) -> str:
    """Show or hide an actor in the scene.

    Args:
        actor_id: The ID (label) of the actor to show or hide.
        visible: True to make the actor visible, False to hide it.

    Returns:
        JSON string with the result including the new visibility state.
    """
    result = await send_command("set_visibility", {
        "actor_id": actor_id,
        "visible": visible,
    })
    return json.dumps(result)


@mcp.tool
async def set_light_intensity(actor_id: str, intensity: float) -> str:
    """Set the brightness of a light actor.

    Works with PointLight, SpotLight, DirectionalLight, and SkyLight actors.
    The intensity is a float scale where 1.0 = default (100%), 0.5 = 50%, 5.0 = 500%.

    Args:
        actor_id: The ID (label) of the light actor.
        intensity: Brightness scale factor (1.0 = default intensity).

    Returns:
        JSON string with the result including the new intensity value.
    """
    result = await send_command("set_light_intensity", {
        "actor_id": actor_id,
        "intensity": intensity,
    })
    return json.dumps(result)


if __name__ == "__main__":
    mcp.run()
