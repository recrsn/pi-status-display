#!/usr/bin/env python3
"""
Fetch JetBrains Mono and regenerate src/ui/fonts/jbmono_*.c LVGL v9 bitmap
fonts via lv_font_conv. Generated files are not committed (see .gitignore) —
this runs automatically before a PlatformIO build (see tools/pio_prebuild.py)
and can also be run by hand after changing a size or codepoint range.

Requires: Node.js (invokes lv_font_conv via npx).

Usage:
    python3 firmware/tools/gen_fonts.py
"""

import io
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

JETBRAINS_MONO_VERSION = "2.304"
RELEASE_URL = (
    "https://github.com/JetBrains/JetBrainsMono/releases/download/"
    f"v{JETBRAINS_MONO_VERSION}/JetBrainsMono-{JETBRAINS_MONO_VERSION}.zip"
)
TTF_MEMBER = "fonts/ttf/JetBrainsMono-Medium.ttf"

FONTS_DIR = Path(__file__).resolve().parent.parent / "src" / "ui" / "fonts"
SIZES = [10, 12, 14, 16, 18, 20]


def fetch_ttf(dest: Path) -> None:
    print(f"==> Downloading JetBrains Mono v{JETBRAINS_MONO_VERSION}", file=sys.stderr)
    with urllib.request.urlopen(RELEASE_URL) as resp:
        data = resp.read()
    with zipfile.ZipFile(io.BytesIO(data)) as zf, zf.open(TTF_MEMBER) as src:
        with open(dest, "wb") as out:
            shutil.copyfileobj(src, out)


def convert(ttf: Path, size: int) -> None:
    out = FONTS_DIR / f"jbmono_{size}.c"
    print(f"==> Generating {out.name}", file=sys.stderr)
    subprocess.run(
        [
            "npx", "--yes", "lv_font_conv",
            "--font", str(ttf),
            "-r", "0x20-0x7E", "-r", "0xB0",
            "--size", str(size),
            "--bpp", "4",
            "--format", "lvgl",
            "--no-compress",
            "--lv-font-name", f"jbmono_{size}",
            # Default fallback path assumes lv_conf.h sits next to a
            # top-level `lvgl/` dir; doesn't match our ESP-IDF managed
            # component layout (managed_components/lvgl__lvgl). The rest of
            # ui/ just includes "lvgl.h" directly (LV_CONF_INCLUDE_SIMPLE).
            "--lv-include", "lvgl.h",
            "-o", str(out),
        ],
        check=True,
    )


def main() -> None:
    FONTS_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        ttf = Path(tmp) / "JetBrainsMono-Medium.ttf"
        fetch_ttf(ttf)
        for size in SIZES:
            convert(ttf, size)


if __name__ == "__main__":
    main()
