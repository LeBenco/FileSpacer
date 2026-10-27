# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
"""Build the optical folder-and-cube icon using Inkscape, Pillow and ImageMagick."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import uuid
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw


NS = "{http://www.w3.org/2000/svg}"
SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
# top, shoulder y, front y, bottom, corner radius, shoulder x, tab x.
# These optical masters describe the small folder, independently of the logo.
FOLDER_MASTERS = {
    16: (1, 3, 4, 15, 1, 7, 11),
    20: (1, 3, 5, 19, 1, 9, 14),
    24: (1, 4, 6, 23, 2, 11, 17),
    32: (2, 5, 8, 30, 2, 15, 23),
    40: (2, 7, 11, 38, 3, 19, 29),
    48: (2, 8, 13, 46, 3, 22, 35),
}


def draw_folder(size):
    """Keep straight edges on-grid; antialias only corners and the tab diagonal."""
    top, shoulder_y, front, bottom, radius, shoulder_x, tab = FOLDER_MASTERS[size]
    factor = 16

    def box(values):
        return tuple(round(v * factor) for v in values)

    back = Image.new("L", (size * factor, size * factor))
    draw = ImageDraw.Draw(back)
    draw.rounded_rectangle(box((0, shoulder_y, size - 1/factor, front + radius)),
                           radius=radius * factor, fill=255)
    draw.rounded_rectangle(box((tab, top, size - 1/factor, front + radius)),
                           radius=radius * factor, fill=255)
    draw.polygon([box(p) for p in ((shoulder_x, shoulder_y), (tab, top),
                 (size - radius, top), (size - radius, front + radius),
                 (shoulder_x, front + radius))], fill=255)
    back = back.resize((size, size), Image.Resampling.BOX)

    face = Image.new("L", (size * factor, size * factor))
    ImageDraw.Draw(face).rounded_rectangle(
        box((0, front, size - 1/factor, bottom - 1/factor)),
        radius=radius * factor, fill=255)
    face = face.resize((size, size), Image.Resampling.BOX)

    image = Image.new("RGBA", (size, size), "#f5b820")
    image.putalpha(back)
    colors = Image.new("RGBA", (size, size))
    for y in range(size):
        t = min(1, max(0, (y - front) / max(1, bottom - front - 1)))
        a, b = (255, 228, 149), (255, 203, 61)
        color = tuple(round(u + (v - u) * t) for u, v in zip(a, b))
        colors.paste((*color, 255), (0, y, size, y + 1))
    colors.putalpha(face)
    return Image.alpha_composite(image, colors)


def draw_logo(size, logo):
    """Rasterize the nine SVG faces on one shared grid, then average once."""
    factor = 32
    high = Image.new("RGBA", (size * factor, size * factor))
    draw = ImageDraw.Draw(high)
    match = re.fullmatch(r"matrix\(([^)]+)\)",
                        logo.get("transform", "matrix(1,0,0,1,0,0)"))
    if match is None:
        raise ValueError("The logo must have an SVG matrix transform.")
    a, b, c, d, e, f = map(float, re.split(r"[,\s]+", match[1].strip()))
    for polygon in logo:
        numbers = list(map(float, re.split(r"[,\s]+", polygon.get("points").strip())))
        points = []
        for x, y in zip(numbers[::2], numbers[1::2]):
            points.append(((a*x+c*y+e) * size/212 * factor,
                           (b*x+d*y+f) * size/212 * factor))
        draw.polygon(points, fill=polygon.get("fill"))
    return high.resize((size, size), Image.Resampling.BOX)


def generate(source, output, work_dir, inkscape):
    artwork = ET.parse(source).getroot()
    if artwork.get("viewBox") != "0 0 212 212":
        raise ValueError("The optical masters require the centered 212 x 212 SVG.")
    logo = artwork.find(f"{NS}g[@id='logo-tesseract-projected']")
    if logo is None or len(logo) != 9 or any(p.tag != NS + "polygon" for p in logo):
        raise ValueError("The SVG must contain the nine projected tesseract polygons.")

    work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="icon-", dir=work_dir) as temporary:
        temporary = Path(temporary)
        frames = []
        for size in SIZES:
            png = temporary / f"FileSpacer-{size}.png"
            if size in FOLDER_MASTERS:
                image = Image.alpha_composite(draw_folder(size), draw_logo(size, logo))
                image.save(png)
            else:
                subprocess.run([inkscape, str(source), f"--export-filename={png}",
                                f"--export-width={size}", f"--export-height={size}",
                                "--export-background-opacity=0"], check=True, timeout=180)
            with Image.open(png) as image:
                rgba = image.convert("RGBA")
            rgba.save(png)
            frames.append(png)

        generated = temporary / "FileSpacer.ico"
        subprocess.run(["magick", *map(str, frames), "-define",
                        "icon:png-compression-size=256", str(generated)],
                       check=True, timeout=180)
        with Image.open(generated) as icon:
            if icon.ico.sizes() != {(size, size) for size in SIZES}:
                raise ValueError("The ICO does not contain all ten expected sizes.")
            for size, png in zip(SIZES, frames):
                with Image.open(png) as image:
                    expected = image.convert("RGBA").tobytes()
                if icon.ico.getimage((size, size)).convert("RGBA").tobytes() != expected:
                    raise ValueError(f"The ICO changed pixels in the {size}px image.")
        # Inherit the resource directory's permissions, not the private temp ACL.
        staged = output.with_name(f".{output.name}.{uuid.uuid4().hex}.tmp")
        try:
            shutil.copyfile(generated, staged)
            staged.replace(output)
        finally:
            staged.unlink(missing_ok=True)
    print(f"Generated {output}: ten sizes, RGBA pixels verified.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--inkscape", required=True)
    args = parser.parse_args()
    generate(args.source.resolve(), args.output.resolve(),
             args.work_dir.resolve(), args.inkscape)
