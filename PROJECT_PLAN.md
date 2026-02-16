# Unreal Engine Agentic Control

> A modular set of tools that enable a user to understand and make changes to a 3D Unreal Engine scene through AI agents via natural language.

---

## 1. Overview

This project provides a natural language interface to the Unreal Engine Editor. Users describe what they want — *"add a red sphere to the centre of the scene"* or *"generate a forest landscape image and apply it to the ground plane"* — and a team of AI agents collaborates to carry out the request inside a live UE session.

### Architecture at a Glance

```
User (natural language)
        │
        ▼
┌──────────────────┐
│  Orchestrator    │  (ADK Agent · Gemini 2.5 Flash)
│  Agent           │  Routes tasks, maintains conversation state
└──────┬───────────┘
       │  delegates to
       ▼
┌──────────────────┐     ┌──────────────────┐
│  UE Editor Agent │     │  Image Gen Agent │
│  (Gemini 2.5     │     │  (Imagen 4)      │
│   Flash)         │     │                  │
└──────┬───────────┘     └──────┬───────────┘
       │                        │
       ▼                        │
┌──────────────────┐            │
│  MCP Server      │◄───────────┘
│  (FastMCP)       │
└──────┬───────────┘
       │  TCP socket
       ▼
┌──────────────────┐
│  UE TCP Plugin   │  (C++ · runs inside UE Editor)
│                  │
└──────────────────┘
```

---

## 2. Components

### 2.1 Orchestrator Agent

| Property | Value |
|---|---|
| Framework | Google ADK (Python) |
| Model | Gemini 2.5 Flash |
| State | Stateful (session-persisted) |

**Responsibilities**

- Receive and interpret user natural language input.
- Decompose complex requests into sub-tasks.
- Delegate sub-tasks to the UE Editor Agent and/or Image Gen Agent.
- Maintain conversation history and session state.
- Return final results / status to the user.

### 2.2 Unreal Engine Editor Agent

| Property | Value |
|---|---|
| Framework | Google ADK (Python) |
| Model | Gemini 2.5 Flash |
| State | Stateful (session-persisted) |

**Responsibilities**

- Receive specific editor tasks from the Orchestrator.
- Invoke MCP server tools to manipulate the UE scene (spawn actors, set transforms, apply materials, query scene state, etc.).
- Report results back to the Orchestrator.

### 2.3 Image Gen Agent

| Property | Value |
|---|---|
| Framework | Google ADK (Python) |
| Model | Imagen 4 |
| State | Stateful (session-persisted) |

**Responsibilities**

- Generate images from text prompts provided by the Orchestrator.
- Save generated images to disk in a format compatible with UE asset import.
- Return image file paths to the Orchestrator for downstream use.

### 2.4 MCP Server

| Property | Value |
|---|---|
| Framework | FastMCP |
| Transport | Stdio (agent ↔ server) |

**Responsibilities**

- Expose a set of tools that agents can call to interact with the UE Editor.
- Translate tool invocations into TCP commands sent to the UE plugin.
- Parse responses from the UE plugin and return structured results.

**Example Tools**

| Tool | Description |
|---|---|
| `spawn_actor` | Spawn a new actor of a given type at a location |
| `set_transform` | Move / rotate / scale an existing actor |
| `apply_material` | Apply a material or texture to a mesh |
| `import_asset` | Import an external file (image, mesh) into the UE project |
| `get_scene_info` | Query the current scene hierarchy and actor details |
| `delete_actor` | Remove an actor from the scene |

### 2.5 Unreal Engine TCP Socket Plugin

| Property | Value |
|---|---|
| Language | C++ |
| Type | UE Editor Plugin |
| Communication | TCP socket server (listens for commands) |

**Responsibilities**

- Run a TCP server inside the UE Editor process.
- Accept structured commands from the MCP server.
- Execute commands using the UE Editor API (spawning actors, modifying properties, importing assets, etc.).
- Return structured responses (success/failure, actor IDs, scene data).

---

## 3. Features

### 3.1 Single-Step Task — Add Elements via Natural Language

**User flow**

1. User says: *"Add a blue cube at position (0, 0, 100)"*
2. Orchestrator interprets the request and delegates to the UE Editor Agent.
3. UE Editor Agent calls MCP tools (`spawn_actor`, `apply_material`, `set_transform`).
4. MCP server sends TCP commands to the UE plugin.
5. The actor appears in the UE viewport.
6. Orchestrator confirms completion to the user.

### 3.2 Multi-Step Task — Generate Image & Apply as Material

**User flow**

1. User says: *"Generate a brick wall texture and apply it to the floor"*
2. Orchestrator decomposes into sub-tasks:
   - **Step A** — Image Gen Agent generates a brick wall image.
   - **Step B** — UE Editor Agent imports the image as a texture, creates a material, and applies it to the floor mesh.
3. Each agent reports back; Orchestrator confirms completion.

---

## 4. Project Structure

```
unreal-engine-agentic-control/
├── PROJECT_PLAN.md
├── README.md
├── .env                          # API keys, config
├── pyproject.toml                # Python project config & dependencies
│
├── agents/
│   ├── __init__.py
│   ├── orchestrator/
│   │   ├── __init__.py
│   │   └── agent.py              # Orchestrator agent definition
│   ├── ue_editor/
│   │   ├── __init__.py
│   │   └── agent.py              # UE Editor agent definition
│   └── image_gen/
│       ├── __init__.py
│       └── agent.py              # Image Gen agent definition
│
├── mcp_server/
│   ├── __init__.py
│   └── server.py                 # FastMCP server & tool definitions
│
├── unreal_plugin/
│   └── AgenticControl/           # UE plugin directory
│       ├── AgenticControl.uplugin
│       └── Source/
│           └── AgenticControl/
│               ├── AgenticControl.Build.cs
│               ├── Public/
│               │   └── AgenticControlServer.h
│               └── Private/
│                   ├── AgenticControlModule.cpp
│                   └── AgenticControlServer.cpp
│
└── tests/
    ├── __init__.py
    └── test_mcp_tools.py
```

---

## 5. Requirements

### 5.1 Functional Requirements

| ID | Requirement |
|---|---|
| FR-01 | Users shall interact with the system via natural language text input. |
| FR-02 | The Orchestrator shall maintain conversation state across turns. |
| FR-03 | The Orchestrator shall decompose multi-step requests and delegate to sub-agents. |
| FR-04 | The UE Editor Agent shall spawn, modify, and delete actors in a live UE scene. |
| FR-05 | The Image Gen Agent shall generate images from text prompts and save them to disk. |
| FR-06 | The MCP server shall expose tools callable by agents for UE Editor operations. |
| FR-07 | The UE plugin shall accept TCP commands and execute them via the Editor API. |
| FR-08 | Generated images shall be importable as UE assets and applicable as materials. |

### 5.2 Non-Functional Requirements

| ID | Requirement |
|---|---|
| NFR-01 | TCP communication between MCP server and UE plugin shall use JSON message format. |
| NFR-02 | The system shall handle agent errors gracefully and report failures to the user. |
| NFR-03 | The UE plugin shall not block the Editor main thread during TCP I/O. |

---

## 6. Compatibility

| Dependency | Version |
|---|---|
| google-adk | 1.25.0 |
| Unreal Engine | 5.7.3 |
| fastmcp | 2.14.5 |
| Python | 3.12+ |

---

## 7. Development Phases

### Phase 1 — Foundation
- [ ] Set up Python project structure and dependencies.
- [ ] Build the UE TCP socket plugin (listen, parse, respond).
- [ ] Implement the MCP server with a minimal tool set (`spawn_actor`, `get_scene_info`).
- [ ] Verify end-to-end: MCP tool call → TCP → actor spawns in UE.

### Phase 2 — Single-Step Agent Tasks
- [ ] Implement the UE Editor Agent with MCP tool access.
- [ ] Implement the Orchestrator Agent with delegation to the UE Editor Agent.
- [ ] Support single-step natural language commands (add / move / delete actors).

### Phase 3 — Image Generation & Multi-Step Tasks
- [ ] Implement the Image Gen Agent (Imagen 4).
- [ ] Add MCP tools for asset import and material application.
- [ ] Wire up the Orchestrator to coordinate multi-step workflows (generate → import → apply).

### Phase 4 — Polish & Extend
- [ ] Add more MCP tools (lighting, camera, landscape, etc.).
- [ ] Improve error handling and user feedback.
- [ ] Add tests for MCP tools and agent behaviour.
