"""UE Editor Agent — invokes MCP tools to manipulate the Unreal Engine scene."""

from __future__ import annotations

import sys
from pathlib import Path

from google.adk.agents import LlmAgent
from google.adk.tools.mcp_tool import McpToolset
from google.adk.tools.mcp_tool.mcp_session_manager import StdioConnectionParams
from mcp import StdioServerParameters

_server_path = str(Path(__file__).resolve().parents[2] / "mcp_server" / "server.py")

ue_editor_toolset = McpToolset(
    connection_params=StdioConnectionParams(
        server_params=StdioServerParameters(
            command=sys.executable,
            args=[_server_path],
        ),
        timeout=30.0,
    ),
)

ue_editor_agent = LlmAgent(
    model="gemini-2.5-flash",
    name="ue_editor",
    instruction=(
        "You are the Unreal Engine Editor agent. You use MCP tools to manipulate "
        "actors in a live UE scene. When asked to add, move, delete, or query objects, "
        "call the appropriate tool:\n"
        "- spawn_actor: Spawn a new actor (StaticMeshActor, PointLight, SpotLight, "
        "DirectionalLight, CameraActor, PlayerStart) at a position.\n"
        "- get_scene_info: Query all actors in the current scene.\n"
        "- delete_actor: Remove an actor by its ID.\n"
        "- set_transform: Move, rotate, or scale an actor (partial updates supported).\n"
        "- import_asset: Import a file from disk into the UE project as an asset.\n"
        "- apply_material: Apply a texture as a material to an actor's mesh.\n\n"
        "Report results clearly and concisely."
    ),
    tools=[ue_editor_toolset],
)
