# Unreal Engine Agentic Control

Natural language control of the Unreal Engine Editor via AI agents. Describe scene changes in plain English and a team of agents collaborates to execute them in a live UE session.

## Architecture

```
User (natural language)
  → Orchestrator Agent (Gemini 2.5 Flash — routes tasks)
    → UE Editor Agent (Gemini 2.5 Flash — calls MCP tools)
      → MCP Server (FastMCP, stdio transport)
        → TCP socket → UE TCP Plugin (C++, runs inside UE Editor)
```

## Quick Start

```bash
# Install dependencies
pip install -e ".[dev]"

# Copy and fill in environment variables
cp .env.example .env

# Run the interactive REPL
python main.py

# Run tests
pytest tests/
```

## Tech Stack

- **Agent framework:** Google ADK (Python) 1.25.0
- **MCP server:** FastMCP 2.14.5
- **LLM:** Gemini 2.5 Flash
- **Engine:** Unreal Engine 5.7.3
- **Python:** 3.12+

## Project Structure

- `agents/` — ADK agent definitions (orchestrator, UE editor, image gen)
- `mcp_server/` — FastMCP server with UE control tools
- `unreal_plugin/` — C++ UE Editor plugin (TCP server)
- `tests/` — pytest test suite
