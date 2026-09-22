# Changelog

All notable user-facing changes to the BB-8 ESPHome component are documented here.

## [0.2.0] - 2026-09-22

### Added

- YAML-defined motion scripts with a button per script, supporting `roll`, `stop`, `spin`, `raw_motor`, `set_heading`, `light`, `back_light`, `center_head`, `stabilize`, `wait`, and `repeat` steps.
- Signed `raw_motor` speeds (-255 to 255), enabling true reverse and pivot moves.
- `battery_voltage` diagnostic sensor (V), updating in all states including while on the charging base.
- `restore_lights_on_connect` option (default `true`): the droid's last light color is re-applied after a reconnect, with Home Assistant entities kept in sync.

### Fixed

- Battery level no longer flaps between the real value and bogus 100% readings; the level derives from measured voltage and holds steady while charging.
- Brightness fades and color changes no longer cancel a running script; Home Assistant light entities re-sync to the droid's actual color when a script finishes and after reconnecting.
- Motion scripts stop when the BLE connection drops and never resume unexpectedly after reconnecting.
- Script packets are rate-limited to the 50 ms command interval (no more bursts that could overwhelm the droid), and `repeat: count` is validated to 1-255.
- A failed connection to a non-BB-8 device no longer stalls forever in "Connecting" - the status sensor reports "Connection failed: not a BB-8" and the Connect button retries cleanly.
- Packet parsing hardened: checksums are validated, corrupt BLE frames trigger a resync instead of a permanent stall, and collision magnitude is computed correctly on hard impacts.
- `center_head` script steps are retried when the link is busy instead of being silently skipped.

### Changed

- Command payloads byte-aligned with the official Android app (Set RGB now uses the app's 3-byte form), with the decompiled app established as the primary protocol reference in the docs.
- Documentation overhauled: README verified against current behavior with a validated example config, project docs consolidated under `docs/`, and host-side test suites added for development.

## [0.1.0] - 2026-02-11

- Initial release: ESP32 BLE bridge for the Sphero BB-8 with main LED and tail light entities, battery/charging status, firmware version reporting, collision detection, and Connect/Disconnect/Center Head buttons.
