# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
"""Derive the installer icon and wizard artwork from FileSpacer's SVG."""

import argparse
import copy
import importlib.util
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw


NS = "{http://www.w3.org/2000/svg}"
ET.register_namespace("", NS[1:-1])


def generate(source, output, inkscape):
    spec = importlib.util.spec_from_file_location(
        "app_icon", Path(__file__).with_name("generate-icon.py"))
    app_icon = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(app_icon)
    artwork = ET.parse(source).getroot()
    if artwork.get("viewBox") != "0 0 212 212":
        raise ValueError("Expected the centered 212 x 212 application artwork.")
    logo = artwork.find(f"{NS}g[@id='logo-tesseract-projected']")
    if logo is None or len(logo) != 9:
        raise ValueError("Expected the nine projected tesseract polygons.")
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

    installer = svg(224, 224)
    ET.SubElement(installer, NS + "use", href="#app", x="6", y="6",
                  width="212", height="212")
    ET.SubElement(installer, NS + "circle", cx="180", cy="181", r="36",
                  fill="#5743A2", stroke="white", **{"stroke-width": "5"})
    # Variant A: enlarged downward arrow within the approved purple badge.
    ET.SubElement(installer, NS + "path", fill="white",
                  d="M172 154h16v26h16l-24 27-24-27h16z")
    frames = []
    for size in app_icon.SIZES:
        png = output / f"installer-{size}.png"
        if size in app_icon.FOLDER_MASTERS:
            frame = Image.alpha_composite(app_icon.draw_folder(size),
                                          app_icon.draw_logo(size, logo))
            factor = 16
            badge = Image.new("RGBA", (size * factor, size * factor))
            draw = ImageDraw.Draw(badge)
            radius = size * .205
            cx, cy = size * .79, size * .8
            draw.ellipse(tuple(round(v * factor) for v in
                         (cx-radius, cy-radius, cx+radius, cy+radius)),
                         fill="#5743A2", outline="white",
                         width=max(1, round(size * .027 * factor)))
            arrow = ((-8, -27), (8, -27), (8, -1), (24, -1),
                     (0, 26), (-24, -1), (-8, -1))
            draw.polygon([((cx+x*radius/36)*factor, (cy+y*radius/36)*factor)
                          for x, y in arrow], fill="white")
            frame = Image.alpha_composite(
                frame, badge.resize((size, size), Image.Resampling.BOX))
            frame.save(png)
        else:
            render(installer, f"installer-{size}", size, size)
        frames.append(png)
    icon_path = output / "FileSpacer-install.ico"
    subprocess.run(["magick", *map(str, frames), "-define",
                    "icon:png-compression-size=256", str(icon_path)],
                   check=True, timeout=180)
    with Image.open(icon_path) as icon:
        if icon.ico.sizes() != {(s, s) for s in app_icon.SIZES}:
            raise ValueError("Installer ICO sizes do not match the application icon.")
        for size, png in zip(app_icon.SIZES, frames):
            with Image.open(png) as frame:
                if icon.ico.getimage((size, size)).convert("RGBA").tobytes() != frame.convert("RGBA").tobytes():
                    raise ValueError(f"Installer ICO pixels changed at {size}px.")

    for language, caption in (("en", "Spatial file manager"),
                              ("fr", "Gestionnaire de fichiers spatial")):
        sidebar = svg(164, 314)
        ET.SubElement(sidebar, NS + "rect", width="164", height="314", fill="#F1F2F6")
        ET.SubElement(sidebar, NS + "use", href="#app", x="23", y="48",
                      width="118", height="118")
        title = ET.SubElement(sidebar, NS + "text", x="82", y="200",
                              fill="#20252E", **{"text-anchor": "middle",
                              "font-family": "Segoe UI", "font-size": "18",
                              "font-weight": "600"})
        title.text = "FileSpacer"
        tagline = ET.SubElement(sidebar, NS + "text", x="82", y="220",
                                fill="#69717C", **{"text-anchor": "middle",
                                "font-family": "Segoe UI", "font-size": "8.5"})
        tagline.text = caption
        png = render(sidebar, f"wizard-{language}", 328, 628)
        with Image.open(png) as image:
            image.convert("RGB").save(output / f"wizard-{language}.bmp")

    header = svg(150, 57)
    ET.SubElement(header, NS + "rect", width="150", height="57", fill="white")
    ET.SubElement(header, NS + "use", href="#app", x="96", y="4", width="49", height="49")
    png = render(header, "header", 300, 114)
    with Image.open(png) as image:
        image.convert("RGB").save(output / "header.bmp")
    print(f"Generated {icon_path}: ten verified sizes; English/French wizard and header bitmaps.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--inkscape", required=True)
    args = parser.parse_args()
    generate(args.source.resolve(), args.output.resolve(), args.inkscape)
