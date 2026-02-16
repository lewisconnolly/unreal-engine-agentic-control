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


if __name__ == "__main__":
    mcp.run()
