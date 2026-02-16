"""CLI entry point — interactive REPL for the Orchestrator agent."""

from __future__ import annotations

import asyncio

from dotenv import load_dotenv
from google.adk.runners import Runner
from google.adk.sessions import InMemorySessionService
from google.genai import types

from agents.orchestrator.agent import orchestrator_agent

APP_NAME = "unreal_agentic_control"
USER_ID = "local_user"


async def main() -> None:
    load_dotenv()

    session_service = InMemorySessionService()
    session = await session_service.create_session(
        app_name=APP_NAME,
        user_id=USER_ID,
    )

    runner = Runner(
        app_name=APP_NAME,
        agent=orchestrator_agent,
        session_service=session_service,
    )

    print("Unreal Engine Agentic Control")
    print("Type your request (Ctrl+C to quit)\n")

    try:
        while True:
            user_input = input("> ").strip()
            if not user_input:
                continue

            content = types.Content(
                role="user",
                parts=[types.Part(text=user_input)],
            )

            async for event in runner.run_async(
                session_id=session.id,
                user_id=USER_ID,
                new_message=content,
            ):
                if event.content and event.content.parts:
                    for part in event.content.parts:
                        if part.text:
                            print(part.text)
    except KeyboardInterrupt:
        print("\nShutting down.")


if __name__ == "__main__":
    asyncio.run(main())
