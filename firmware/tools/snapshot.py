#!/usr/bin/env python3
"""
Convert the raw PPM dumped by hal_sdl.c's snapshot hook (press 's' in the
running emulator) into a downscaled PNG for docs/README screenshots.

Usage:
    python3 firmware/tools/snapshot.py [-i INPUT.ppm] [-o OUTPUT.png] [--scale 0.5]
"""

import argparse
from pathlib import Path

from PIL import Image

DEFAULT_INPUT = Path("/tmp/pi-status-snapshot.ppm")
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent / "docs" / "emulator.png"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("-o", "--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--scale", type=float, default=0.5, help="downscale factor (default: 0.5, i.e. 2x down)")
    args = parser.parse_args()

    if not args.input.exists():
        raise SystemExit(f"{args.input} not found — press 's' in the running emulator first")

    img = Image.open(args.input)
    orig_size = img.size
    size = (round(img.width * args.scale), round(img.height * args.scale))
    img = img.resize(size, Image.LANCZOS)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.output)
    print(f"{args.input} ({orig_size[0]}x{orig_size[1]}) -> {args.output} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
