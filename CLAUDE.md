# CLAUDE.md

## Project Purpose

Natural language control of the Unreal Engine Editor via AI agents. Users describe scene changes in plain English and a team of agents collaborates to execute them in a live UE session.

## Architecture

```
User (natural language)
  → Orchestrator Agent (routes tasks, maintains conversation state)
    → UE Editor Agent (calls MCP tools to manipulate the scene)
    → Image Gen Agent (generates images from text prompts)
      → MCP Server (FastMCP, stdio transport)
        → TCP socket
          → UE TCP Plugin (C++, runs inside UE Editor)
```

- **Orchestrator Agent** — Decomposes requests, delegates to sub-agents, maintains session state.
- **UE Editor Agent** — Invokes MCP tools to spawn/modify/delete actors, apply materials, query scene.
- **Image Gen Agent** — Generates images via Imagen 4, saves to disk for UE import.
- **MCP Server** — Exposes tools (spawn_actor, set_transform, apply_material, import_asset, get_scene_info, delete_actor) over stdio. Translates calls into JSON TCP commands.
- **UE TCP Plugin** — C++ Editor plugin. Listens on TCP, executes commands via UE Editor API, returns JSON responses. Must not block the Editor main thread.

## Tech Stack

| Component | Technology | Version |
|---|---|---|
| Agent framework | Google ADK (Python) | 1.25.0 |
| MCP server | FastMCP | 2.14.5 |
| Python | | 3.12+ |
| Unreal Engine | | 5.7.3 |
| Orchestrator/UE Editor model | Gemini 2.5 Flash | |
| Image generation model | Imagen 4 | |

## Project Structure

```
agents/
  orchestrator/agent.py    # Orchestrator agent definition
  ue_editor/agent.py       # UE Editor agent definition
  image_gen/agent.py       # Image Gen agent definition
mcp_server/
  server.py                # FastMCP server & tool definitions
unreal_plugin/
  AgenticControl/          # UE C++ plugin (AgenticControl.uplugin)
tests/
  test_mcp_tools.py
```

**Entry points:** `agents/orchestrator/agent.py` is the main entry point. The MCP server runs via `mcp_server/server.py`. The UE plugin loads inside the Editor.

## Development Setup

1. **Python 3.12+** — Install dependencies via `pyproject.toml`
2. **Unreal Engine 5.7.3** — Copy `unreal_plugin/AgenticControl/` into your UE project's `Plugins/` directory
3. **Google API keys** — Set in `.env` for Gemini 2.5 Flash and Imagen 4 access
4. **Dependencies** — `google-adk==1.25.0`, `fastmcp==2.14.5`

## Key Patterns

- Agents are defined using Google ADK's Python SDK, each in its own module under `agents/`.
- MCP tools use FastMCP decorators in `mcp_server/server.py`.
- Agent-to-MCP communication uses stdio transport. MCP-to-UE uses TCP with JSON messages.
- All agents are stateful with session-persisted state.

## Development Phases

The project follows a phased approach (see PROJECT_PLAN.md Section 7):
1. **Foundation** — Python project structure, UE TCP plugin, minimal MCP tools, end-to-end verification
2. **Single-Step Tasks** — UE Editor Agent, Orchestrator, single-step NL commands
3. **Image Gen & Multi-Step** — Image Gen Agent, asset import tools, multi-step workflows
4. **Polish** — More tools, error handling, tests

## Reference

See `PROJECT_PLAN.md` for detailed requirements, component specifications, and feature descriptions.
