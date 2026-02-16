"""UE Editor Agent — invokes MCP tools to manipulate the Unreal Engine scene."""

from __future__ import annotations

import sys
from pathlib import Path

from google.adk.agents import LlmAgent
from google.adk.tools.mcp_tool import McpToolset
from google.adk.tools.mcp_tool.mcp_session_manager import StdioConnectionParams
from mcp import StdioServerParameters

_server_path = str(Path(__file__).resolve().parents[2] / "mcp_server" / "server.py")

ue_editor_agent = LlmAgent(
    model="gemini-2.5-flash",
    name="ue_editor",
    instruction=(
        "You are the Unreal Engine Editor agent. You use MCP tools to manipulate "
        "actors in a live UE scene. When asked to add, move, or query objects, call "
        "the appropriate tool (spawn_actor, get_scene_info). Report results clearly."
    ),
    tools=[
        McpToolset(
            connection_params=StdioConnectionParams(
                server_params=StdioServerParameters(
                    command=sys.executable,
                    args=[_server_path],
                ),
            ),
        )
    ],
)
