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


if __name__ == "__main__":
    mcp.run()
