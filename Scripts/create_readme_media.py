"""Build compact README previews from checked-in gameplay captures.

The output is intentionally assembled from real in-game screenshots.  Keeping the
recipe in the repository makes the README media reproducible when captures change.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MEDIA_ROOT = PROJECT_ROOT / "Docs" / "Media"
OUTPUT_PATH = MEDIA_ROOT / "readme-route-preview.gif"
FRAME_SIZE = (768, 432)
PALETTE_COLORS = 160

CAPTURES = (
    "prologue-bedroom.png",
    "prologue-kitchen.png",
    "prologue-corridor.png",
    "prologue-elevator.png",
    "prologue-lobby.png",
    "prologue-villa.png",
    "prologue-alley.png",
    "prologue-store.png",
)


def load_capture(filename: str) -> Image.Image:
    source_path = MEDIA_ROOT / filename
    if not source_path.is_file():
        raise FileNotFoundError(f"README capture is missing: {source_path}")

    with Image.open(source_path) as source:
        return ImageOps.fit(
            source.convert("RGB"),
            FRAME_SIZE,
            method=Image.Resampling.LANCZOS,
        )


def build_global_palette(frames: list[Image.Image]) -> Image.Image:
    samples = [frame.resize((192, 108), Image.Resampling.LANCZOS) for frame in frames]
    atlas = Image.new("RGB", (192, 108 * len(samples)))
    for index, sample in enumerate(samples):
        atlas.paste(sample, (0, index * 108))
    return atlas.quantize(colors=PALETTE_COLORS, method=Image.Quantize.MEDIANCUT)


def main() -> None:
    captures = [load_capture(filename) for filename in CAPTURES]
    palette = build_global_palette(captures)

    rgb_frames: list[Image.Image] = []
    durations: list[int] = []
    for index, current in enumerate(captures):
        rgb_frames.append(current)
        durations.append(700)

        if index == len(captures) - 1:
            continue

        following = captures[index + 1]
        for step in range(1, 5):
            rgb_frames.append(Image.blend(current, following, step / 5.0))
            durations.append(80)

    gif_frames = [
        frame.quantize(palette=palette, dither=Image.Dither.FLOYDSTEINBERG)
        for frame in rgb_frames
    ]
    gif_frames[0].save(
        OUTPUT_PATH,
        save_all=True,
        append_images=gif_frames[1:],
        duration=durations,
        loop=0,
        optimize=True,
        disposal=2,
        comment=b"Actual in-game capture route preview",
    )
    print(
        "README_MEDIA PASS "
        f"captures={len(captures)} frames={len(gif_frames)} output={OUTPUT_PATH}"
    )


if __name__ == "__main__":
    main()
