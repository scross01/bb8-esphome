# Gemini Context: Sphero BB-8 ESPHome Component

This `GEMINI.md` file provides essential context for AI agents interacting with the Sphero BB-8 ESPHome project.

## Project Overview

*   **Purpose:** A custom ESPHome C++ component to control a Sphero BB-8 droid via Bluetooth Low Energy (BLE) using an ESP32.
*   **Key Technologies:** ESPHome, ESP-IDF Framework, C++, Bluetooth Low Energy (BLE).
*   **Architecture:**
    *   **Hub (`SpheroBB8`):** Manages BLE connection, initialization handshake, and packet transmission.
    *   **Light Platform:** Controls RGB and Back LEDs.
    *   **Button Platform:** Provides manual Connect and Disconnect actions.
    *   **Text Sensor Platform:** Reports connection status (Disconnected, Connecting, Initializing, Ready, Disabling).

## Building and Running

*   **Build System:** ESPHome CLI.
*   **Compile Command:** `esphome compile <config_file.yaml>`
*   **Run Command:** `esphome run <config_file.yaml>` (compiles, uploads, and monitors logs).

## Development Conventions

*   **Language:** C++ (ESP-IDF style).
*   **Logging:** Uses `ESP_LOGx` macros. Packet transmission logs are at `VERBOSE` level.
*   **State Management:** Tracks `target` vs `current` state to ensure synchronization. Enforces a 50ms rate limit for unacknowledged packets.
*   **BLE Protocol:** Strict initialization sequence (Subscribe -> Anti-DOS -> TX Power -> Wake -> Stabilize). Uses temporary flags for RGB to avoid flash wear.

## Key Files

*   **`components/sphero_bb8/sphero_bb8.h/cpp`:** Core logic for the hub and packet encoding.
*   **`components/sphero_bb8/sphero_bb8_light.h/cpp`:** Light platform implementation.
*   **`components/sphero_bb8/button.py`:** Python configuration for Connect/Disconnect buttons.
*   **`components/sphero_bb8/text_sensor.py`:** Python configuration for the connection status sensor.
*   **`DEVELOPMENT.md`:** Deep technical documentation on architecture and protocol.
*   **`README.md`:** User-facing documentation and usage instructions.