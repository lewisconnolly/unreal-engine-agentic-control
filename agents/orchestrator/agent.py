"""Orchestrator Agent — routes user requests to specialised sub-agents."""

from __future__ import annotations

from google.adk.agents import LlmAgent

from agents.ue_editor.agent import ue_editor_agent

orchestrator_agent = LlmAgent(
    model="gemini-2.5-flash",
    name="orchestrator",
    instruction=(
        "You are the Orchestrator. You receive natural language requests from the user "
        "about an Unreal Engine scene. Delegate UE scene manipulation tasks to the "
        "ue_editor agent. Summarise results for the user.\n\n"
        "Available sub-agents:\n"
        "- ue_editor: Spawns, modifies, deletes actors and queries scene state."
    ),
    sub_agents=[ue_editor_agent],
)
