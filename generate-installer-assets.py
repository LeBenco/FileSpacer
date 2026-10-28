# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
"""Compose optical installer icons from FileSpacer's ICO; render wizard SVGs."""

import argparse
import copy
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw


NS = "{http://www.w3.org/2000/svg}"
ET.register_namespace("", NS[1:-1])
# Diameter, white outline, arrow shaft width, head width and total height.
# These are pixel measurements for each native frame, not scaled SVG geometry.
BADGE_MASTERS = {
    16: (7, .75, 1, 4, 5),
    20: (8, 1, 1.5, 4.5, 5.5),
    24: (10, 1, 2, 5.5, 7),
    32: (13, 1.25, 2.5, 7.5, 9),
    40: (16, 1.5, 3, 9.5, 11),
    48: (19, 1.75, 3.5, 11, 13),
    64: (24, 2, 4.5, 14.5, 16.5),
    96: (34, 2.5, 6.5, 21, 23.5),
    128: (45, 3.25, 8.5, 28, 31),
    256: (88, 6, 18, 55, 61),
}


def draw_badge(size, bounds):
    """Fit an independent badge to this bitmap's folder edges with a yellow rim."""
    diameter, stroke, shaft, head, height = BADGE_MASTERS[size]
    factor = 16
    right, bottom = bounds[2] - 1 - 1/factor, bounds[3] - 1 - 1/factor
    cx, cy = right - diameter/2, bottom - diameter/2
    badge = Image.new("RGBA", (size * factor, size * factor))
    draw = ImageDraw.Draw(badge)
    draw.ellipse(tuple(round(v * factor) for v in
                 (right-diameter, bottom-diameter, right, bottom)),
                 fill="#5743A2", outline="white", width=round(stroke * factor))
    # Snap the shaft's left edge to a pixel, keeping its own optical width.
    left = round(cx - shaft/2)
    ax = left + shaft/2
    arrow = ((left, cy-height/2), (left+shaft, cy-height/2),
             (left+shaft, cy), (ax+head/2, cy), (ax, cy+height/2),
             (ax-head/2, cy), (left, cy))
    draw.polygon([(x*factor, y*factor) for x, y in arrow], fill="white")
    return badge.resize((size, size), Image.Resampling.BOX)


def generate(source, icon_source, output, inkscape):
    artwork = ET.parse(source).getroot()
    if artwork.get("viewBox") != "0 0 212 212":
        raise ValueError("Expected the centered 212 x 212 application artwork.")
    output.mkdir(parents=True, exist_ok=True)

    def svg(width, height):
        root = ET.Element(NS + "svg", viewBox=f"0 0 {width} {height}",
                          width=str(width), height=str(height))
        symbol = ET.SubElement(ET.SubElement(root, NS + "defs"), NS + "symbol",
                               id="app", viewBox="0 0 212 212", fill="none")
        for child in artwork:
            if child.tag.startswith(NS) and child.tag != NS + "metadata":
                symbol.append(copy.deepcopy(child))
        return root

    def render(root, name, width, height):
        vector, png = output / f"{name}.svg", output / f"{name}.png"
        ET.ElementTree(root).write(vector, encoding="utf-8", xml_declaration=True)
        subprocess.run([inkscape, str(vector), f"--export-filename={png}",
                        f"--export-width={width}", f"--export-height={height}"],
                       check=True, timeout=180)
        return png

    frames = []
    with Image.open(icon_source) as app:
        if app.ico.sizes() != {(s, s) for s in BADGE_MASTERS}:
            raise ValueError("The application ICO must contain all ten optical sizes.")
        for size in BADGE_MASTERS:
            base = app.ico.getimage((size, size)).convert("RGBA")
            badge = draw_badge(size, base.getbbox())
            frame = Image.alpha_composite(base, badge)
            if frame.getchannel("A").tobytes() != base.getchannel("A").tobytes():
                raise ValueError(f"The badge extends outside the folder at {size}px.")
            png = output / f"installer-{size}.png"
            frame.save(png)
            badge.save(output / f"badge-{size}.png")
            frames.append(png)
    icon_path = output / "FileSpacer-install.ico"
    subprocess.run(["magick", *map(str, frames), "-define",
                    "icon:png-compression-size=256", str(icon_path)],
                   check=True, timeout=180)
    with Image.open(icon_path) as icon:
        if icon.ico.sizes() != {(s, s) for s in BADGE_MASTERS}:
            raise ValueError("Installer ICO sizes do not match the application icon.")
        for size, png in zip(BADGE_MASTERS, frames):
            with Image.open(png) as frame:
                if icon.ico.getimage((size, size)).convert("RGBA").tobytes() != frame.convert("RGBA").tobytes():
                    raise ValueError(f"Installer ICO pixels changed at {size}px.")

    # Branding text is drawn by native, DPI-scaled controls in the wizard.
    for language in ("en", "fr"):
        sidebar = svg(164, 176)
        ET.SubElement(sidebar, NS + "rect", width="164", height="176", fill="#F1F2F6")
        ET.SubElement(sidebar, NS + "use", href="#app", x="23", y="48",
                      width="118", height="118")
        png = render(sidebar, f"wizard-{language}", 328, 352)
        with Image.open(png) as image:
            image.convert("RGB").save(output / f"wizard-{language}.bmp")

    print(f"Generated {icon_path}: ten verified sizes and English/French sidebar bitmaps.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--icon", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--inkscape", required=True)
    args = parser.parse_args()
    generate(args.source.resolve(), args.icon.resolve(), args.output.resolve(), args.inkscape)
