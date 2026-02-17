"""Orchestrator Agent — routes user requests to specialised sub-agents."""

from __future__ import annotations

from google.adk.agents import LlmAgent

from agents.ue_editor.agent import ue_editor_agent
from agents.image_gen.agent import image_gen_agent

orchestrator_agent = LlmAgent(
    model="gemini-3-flash-preview",
    name="orchestrator",
    instruction=(
        "You are the Orchestrator. You receive natural language requests from the user "
        "about an Unreal Engine scene. Delegate UE scene manipulation tasks to the "
        "ue_editor agent. Delegate image generation tasks to the image_gen agent. "
        "For multi-step tasks (e.g. generate a texture and apply it to an actor), "
        "coordinate: first use image_gen to generate the image, then use ue_editor "
        "to import the asset and apply the material. Summarise results for the user.\n\n"
        "Available sub-agents:\n"
        "- ue_editor: Spawns, modifies, deletes actors, queries scene state, imports assets, "
        "and applies materials.\n"
        "- image_gen: Generates images from text descriptions."
    ),
    sub_agents=[ue_editor_agent, image_gen_agent],
)
