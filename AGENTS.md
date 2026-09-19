# AGENTS.md

Guidance for AI agents working in this repository.

## Project Overview

Custom ESPHome component (`components/sphero_bb8/`) that lets an ESP32 control a Sphero BB-8 droid over Bluetooth Low Energy, exposing it as Home Assistant entities. C++ on the ESP-IDF framework, plus Python for ESPHome config validation and codegen.

## Verification Commands

- Agent-safe validation: `esphome config test.yaml` (use `test_motion_control.yaml` when touching script features) — validates YAML + codegen without touching hardware.
- **NEVER run `esphome compile` or `esphome run` directly** — ask the user to execute and report results.
  - Compile: `esphome compile <config.yaml>`
  - Run (compile + upload + monitor): `esphome run <config.yaml>`
- Do not attempt to read PDF files (e.g. `reference/spherov2-readthedocs-io-en-latest.pdf`); use the converted markdown `reference/spherov2-readthedocs-io-en-latest.md` instead (note: it is a signatures-only API listing — no byte offsets or enum values). Also see `docs/REFERENCE.md` and `docs/DEVELOPMENT.md`.

## Architecture

Component package: `components/sphero_bb8/`

- **Hub** (`sphero_bb8.h/cpp`): BLE connection lifecycle, initialization handshake, packet encoding/checksum, response parsing.
- **Script executor** (`script_executor.h/cpp`): non-blocking motion-script engine (`roll`, `wait`, `repeat`, …) pumped from the hub `loop()`; manual commands stop running scripts.
- **Light** (`sphero_bb8_light.h/cpp`): RGB LED and taillight output.
- **Python config** (`__init__.py`, `button.py`, `light.py`, `sensor.py`, `binary_sensor.py`, `text_sensor.py`, `action.py`): ESPHome platform registration, codegen, and YAML motion-script parsing.

Docs of record: `docs/DEVELOPMENT.md` (architecture + protocol deep dive), `docs/REFERENCE.md` (protocol quick reference), `README.md` (user docs), `docs/MOTION_CONTROL.md` (script engine design decisions). `plans/` holds audited implementation plans — consult before refactoring the same areas.

## Critical BLE Protocol (Non-Obvious)

- **Initialization sequence MUST be exact**: Subscribe → Anti-DOS → TX Power → Wake → Stabilize (wait `stabilize_wait_ms`, default 500ms).
- **Write types matter**: Use `ESP_GATT_WRITE_TYPE_RSP` for init (sequential), `ESP_GATT_WRITE_TYPE_NO_RSP` for light control (smooth animations).
- **Rate limiting**: 50ms minimum between packets when using `NO_RSP`.
- **State sync**: Track `target_*` vs `current_*` values; initialize current to `0xFE` (invalid) to force sync on connect.
- **RGB persist flag**: use `0x00` (temporary) to avoid flash wear.
- **Keep-alive**: Ping every 2 seconds or droid sleeps/disconnects.

## Code Conventions

- Logging: Use `ESP_LOGx` macros (packet transmission at VERBOSE level).
- Namespace: `esphome::sphero_bb8`.
- Checksum: `~(sum(DID + CID + SEQ + DLEN + DATA) % 256)`.

## Reference Implementations

- **gobot** — Go libraries with BB-8 support: `reference/gobot/`
- **Sphero V2 Python API docs** — `reference/spherov2-readthedocs-io-en-latest.md` (converted from PDF; signatures only — no byte offsets or enum values). Do not parse the PDF original.
- **Sphero Android APK** — decompiled: `reference/apktool/`
