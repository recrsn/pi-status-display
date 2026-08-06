#!/usr/bin/env python3
"""
Regenerate src/ui/icons/*.c from Lucide (https://lucide.dev) SVGs.

Each icon is fetched from unpkg, tinted opaque white, rasterized to a fixed
size, and emitted as an LVGL v9 A8 (alpha-only) lv_image_dsc_t — the actual
RGB is irrelevant since screens tint icons at runtime via
lv_obj_set_style_image_recolor (see icons.h).

Requires: rsvg-convert (`brew install librsvg`), Pillow (`pip install pillow`).

Usage:
    python3 firmware/tools/gen_icons.py            # regenerate all icons
    python3 firmware/tools/gen_icons.py icon_cpu    # regenerate just one
"""

import subprocess
import sys
import urllib.request
from pathlib import Path

from PIL import Image

ICONS_DIR = Path(__file__).resolve().parent.parent / "src" / "ui" / "icons"
LUCIDE_BASE = "https://unpkg.com/lucide-static@latest/icons"

# our name -> (lucide icon name, output size in px)
ICONS = {
    "icon_cpu": ("cpu", 20),
    "icon_mem": ("memory-stick", 20),
    "icon_disk": ("hard-drive", 20),
    "icon_temp": ("thermometer", 20),
    "icon_chevron_up": ("chevron-up", 12),
    "icon_chevron_down": ("chevron-down", 12),
    "icon_wifi": ("wifi", 12),
    "icon_eth": ("ethernet-port", 12),
    "icon_server": ("server", 12),
}

TEMPLATE = """#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_ICON_{upper}
#define LV_ATTRIBUTE_ICON_{upper}
#endif

static const
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_ICON_{upper}
uint8_t {name}_map[] = {{

{rows}
}};

const lv_image_dsc_t {name} = {{
  .header = {{
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_A8,
    .flags = 0,
    .w = {size},
    .h = {size},
    .stride = {size},
    .reserved_2 = 0,
  }},
  .data_size = sizeof({name}_map),
  .data = {name}_map,
  .reserved = NULL,
}};
"""


def fetch_svg(lucide_name: str) -> str:
    url = f"{LUCIDE_BASE}/{lucide_name}.svg"
    with urllib.request.urlopen(url) as resp:
        svg = resp.read().decode("utf-8")
    return svg.replace('stroke="currentColor"', 'stroke="#ffffff"')


def rasterize(svg: str, size: int) -> Image.Image:
    png_bytes = subprocess.run(
        ["rsvg-convert", "-w", str(size), "-h", str(size), "-a", "--background-color=none"],
        input=svg.encode("utf-8"),
        capture_output=True,
        check=True,
    ).stdout
    import io
    return Image.open(io.BytesIO(png_bytes)).convert("RGBA")


def emit_c_file(name: str, size: int, img: Image.Image, out_path: Path) -> None:
    assert img.size == (size, size), f"{name}: expected {size}x{size}, got {img.size}"
    alpha = img.getchannel("A").tobytes()
    rows = [
        "    " + ",".join(f"0x{v:02x}" for v in alpha[y * size:(y + 1) * size]) + ","
        for y in range(size)
    ]
    upper = name.upper().replace("ICON_", "")
    out_path.write_text(TEMPLATE.format(upper=upper, name=name, rows="\n".join(rows), size=size))


def main() -> None:
    requested = sys.argv[1:] or list(ICONS.keys())
    for name in requested:
        if name not in ICONS:
            print(f"unknown icon: {name} (known: {', '.join(ICONS)})", file=sys.stderr)
            sys.exit(1)
        lucide_name, size = ICONS[name]
        svg = fetch_svg(lucide_name)
        img = rasterize(svg, size)
        out_path = ICONS_DIR / f"{name}.c"
        emit_c_file(name, size, img, out_path)
        print(f"{name} <- lucide:{lucide_name} ({size}x{size}) -> {out_path}")


if __name__ == "__main__":
    main()
