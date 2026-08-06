# pi-status-display

Plug-and-play USB status display for a Raspberry Pi: a Go agent on the Pi pushes system
status over USB CDC to an ESP32-S3 running an LVGL UI on a 1.69" ST7789V2 touch display.

```
Raspberry Pi  --[agent]-->  USB CDC (JSON, ~1 Hz)  -->  ESP32-S3 + ST7789V2/CST816  --[LVGL UI]
```

![Emulator screenshot](firmware/docs/emulator.png)

## Layout

- `schema/` — JSON Schema, source of truth for the wire protocol (`status.schema.json`, `command.schema.json`)
- `agent/` — Go daemon that collects Pi metrics and pushes them over serial/socket
- `firmware/` — PlatformIO project for the display; `esp32s3` env targets the real hardware,
  `native` env builds an SDL2 emulator for development without a board

## Protocol

Newline-delimited JSON, push-only from Pi to ESP32, additive-only schema (`v` field for
versioning; unknown keys must be ignored by consumers). Commands flow ESP32 → Pi as
fire-and-forget `{"cmd": "reboot|shutdown|refresh"}`, confirmed by long-press on the device.

## Agent

```
cd agent
go build ./cmd/pi-status-agent
```

Config lives at `/etc/pi-status/config.yaml` (see `agent/config.example.yaml`) — serial port,
socket path, poll interval, alert thresholds, and the list of systemd services to monitor.
Packaging for a `.deb` with systemd unit and udev rule (`/dev/pi-display` symlink by VID/PID)
is under `agent/packaging/`.

Run locally without hardware:

```
./pi-status-agent --local
```

## Firmware / emulator

Requires [PlatformIO](https://platformio.org/).

```
cd firmware
pio run -e esp32s3 -t upload   # flash real hardware
pio run -e native              # build SDL2 emulator (needs sdl2, libcjson via pkg-config)
.pio/build/native/program
```

The emulator connects to the same Unix socket the agent writes to
(`/tmp/pi-status.sock` by default), so you can develop the UI without a board attached.
