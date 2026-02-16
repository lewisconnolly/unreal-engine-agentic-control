"""Image Gen Agent — generates images from text prompts using Imagen 4."""

from __future__ import annotations

import os
from pathlib import Path

from google import genai
from google.adk.agents import LlmAgent

OUTPUT_DIR = os.getenv("IMAGE_GEN_OUTPUT_DIR", "./generated_images")
IMAGE_GEN_MODEL = os.getenv("IMAGE_GEN_MODEL", "imagen-4.0-generate-001")


def generate_image(prompt: str, filename: str) -> str:
    """Generate an image from a text prompt and save it to disk.

    Args:
        prompt: A detailed text description of the image to generate.
        filename: The filename (without extension) to save the image as.

    Returns:
        The absolute file path of the saved PNG image.
    """
    client = genai.Client()

    response = client.models.generate_images(
        model=IMAGE_GEN_MODEL,
        prompt=prompt,
        config=genai.types.GenerateImagesConfig(number_of_images=1),
    )

    output_dir = Path(OUTPUT_DIR)
    output_dir.mkdir(parents=True, exist_ok=True)

    output_path = output_dir / f"{filename}.png"
    output_path.write_bytes(response.generated_images[0].image.image_bytes)

    return str(output_path.resolve())


image_gen_agent = LlmAgent(
    model="gemini-2.5-flash",
    name="image_gen",
    instruction=(
        "You are the Image Generation agent. You generate images from text descriptions "
        "using the generate_image tool.\n\n"
        "When you receive a request:\n"
        "1. Interpret the user's intent and craft a detailed, optimised prompt for image generation.\n"
        "2. Choose a descriptive filename (lowercase, underscores, no extension).\n"
        "3. Call generate_image with the prompt and filename.\n"
        "4. Return the absolute file path of the generated image."
    ),
    tools=[generate_image],
)
