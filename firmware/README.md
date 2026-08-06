# firmware

PlatformIO project for the ESP32-S3 display. See the top-level README for the build/flash
commands and overall protocol.

## Layout

- `src/` — LVGL UI, protocol parsing, HAL (real hardware + native/SDL2)
- `include/`, `sdkconfig.defaults`, `sdkconfig.esp32s3` — ESP-IDF config
- `docs/` — assets referenced from READMEs (e.g. `emulator.png`)
- `tools/` — one-off scripts, not part of the build:
  - `gen_icons.py` — regenerates `src/ui/icons/*.c` from Lucide SVGs (fetched from unpkg,
    rasterized to LVGL A8 image descriptors). Run after adding or changing an icon.
    Requires `rsvg-convert` (`brew install librsvg`) and Pillow (`pip install pillow`).

    ```
    python3 tools/gen_icons.py            # regenerate all icons
    python3 tools/gen_icons.py icon_cpu   # regenerate just one
    ```

  - `snapshot.py` — converts the raw PPM dumped by the native emulator (press `s` while it's
    running) into the downscaled `docs/emulator.png` used in the top-level README.

    ```
    python3 tools/snapshot.py [-i INPUT.ppm] [-o OUTPUT.png] [--scale 0.5]
    ```
