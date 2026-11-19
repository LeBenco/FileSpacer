# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
"""Create lossless website variants; the GIFs remain the Markdown sources."""
import hashlib
import json
from pathlib import Path

from PIL import Image


def sha256(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def optimize(root):
    manifest = {}
    total_before = total_after = 0
    for source in sorted((root / "docs").glob("*/images/*.gif")):
        output = source.with_suffix(".webp")
        temporary = output.with_suffix(".webp.tmp")
        frames, durations = [], []
        try:
            with Image.open(source) as gif:
                repetitions = gif.info.get("loop")
                loop = 0 if repetitions == 0 else (repetitions + 1 if repetitions is not None else 1)
                for index in range(gif.n_frames):
                    gif.seek(index)
                    frames.append(gif.convert("RGBA"))
                    durations.append(gif.info["duration"])
            frames[0].save(temporary, format="WEBP", save_all=True,
                           append_images=frames[1:], duration=durations, loop=loop,
                           lossless=True, method=6, minimize_size=True)
            # Verify decoded pixels, timing and looping before publishing a variant.
            with Image.open(temporary) as webp:
                if webp.n_frames != len(frames) or webp.info.get("loop") != loop:
                    raise ValueError(f"Animation structure changed: {source}")
                for index, original in enumerate(frames):
                    webp.seek(index)
                    decoded = webp.convert("RGBA")
                    if decoded.tobytes() != original.tobytes() or webp.info["duration"] != durations[index]:
                        raise ValueError(f"Animation pixels or timing changed: {source}, frame {index}")
            if temporary.stat().st_size >= source.stat().st_size:
                print(f"KEEP GIF {source.relative_to(root)}", flush=True)
                continue
            temporary.replace(output)
            manifest[source.relative_to(root).as_posix()] = sha256(source)
            total_before += source.stat().st_size
            total_after += output.stat().st_size
            print(f"{source.relative_to(root)}: {source.stat().st_size} -> {output.stat().st_size}", flush=True)
        finally:
            temporary.unlink(missing_ok=True)
            for frame in frames:
                frame.close()
    (root / "website/animations.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(manifest)} animations: {total_before} -> {total_after} bytes", flush=True)


if __name__ == "__main__":
    optimize(Path(__file__).resolve().parents[2])
