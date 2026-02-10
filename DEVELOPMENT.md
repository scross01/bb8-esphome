# Development Guide & Architecture

This document provides detailed technical information about the internal workings of the Sphero BB-8 ESPHome component. It is intended for developers and AI agents who wish to extend, maintain, or debug the project.

## Architecture Overview

The project is built as a custom **C++ component** for ESPHome, leveraging the **ESP-IDF framework**. It acts as a bridge between Home Assistant (via ESPHome's API) and the Sphero BB-8 droid (via Bluetooth Low Energy).

### Component Structure

The component is split into two main parts:

1.  **`SpheroBB8` (Hub)**:
    *   Inherits from `esphome::Component` and `esphome::ble_client::BLEClientNode`.
    *   Manages the BLE connection lifecycle.
    *   Handles the complex initialization handshake (Anti-DOS, Wake).
    *   Maintains the state machine for packet transmission.
    *   Implements the raw Sphero API packet encoding.

2.  **`SpheroBB8Light` (Platform)**:
    *   Inherits from `esphome::light::LightOutput`.
    *   Translates ESPHome light state (RGB, Brightness) into Sphero commands.
    *   Uses immediate state logic (`remote_values`) to bypass internal fading, allowing the hub to manage rate limiting.

## Sphero BB-8 BLE Protocol

The Sphero BB-8 uses a proprietary BLE protocol. Understanding this protocol is key to extending the component.

### BLE Services & Characteristics

The droid exposes several services, but two are critical for control:

| Service Name | UUID Segment | Full UUID | Purpose |
| :--- | :--- | :--- | :--- |
| **BLE Service** | `2bb0` | `22bb746f-2bb0-7554-2d6f-726568705327` | Connection management, Anti-DOS, Wake. |
| **Control Service** | `2ba0` | `22bb746f-2ba0-7554-2d6f-726568705327` | Sending commands (Roll, RGB) and receiving async packets. |

**Key Characteristics:**

*   **Anti-DOS** (`2bbd`): Used to unlock the device. Write `"011i3"`.
*   **TX Power** (`2bb2`): Sets radio power. Typically set to `7`.
*   **Wake** (`2bbf`): Wakes the main processor. Write `0x01`.
*   **Commands** (`2ba1`): The primary endpoint for sending packetized commands.
*   **Responses** (`2ba6`): The endpoint for receiving ACKs and Async messages (collisions, sensor data). **Must be subscribed to.**

### The Initialization Handshake

The BB-8 will **not** accept commands and may disconnect if this specific sequence is not followed immediately after connection:

1.  **Subscribe** to `Responses` characteristic (`2ba6`). *Critical: prevents internal buffer overflow on the robot.*
2.  **Send Anti-DOS**: Write string `"011i3"` to `2bbd`.
3.  **Set TX Power**: Write byte `0x07` to `2bb2`.
4.  **Wake**: Write byte `0x01` to `2bbf`.

*Note: All initialization writes should use `ESP_GATT_WRITE_TYPE_RSP` (Write with Response) to ensure sequential execution.*

### Packet Structure

Commands sent to the **Commands Characteristic** (`2ba1`) follow this binary structure:

```
[SOP1, SOP2, DID, CID, SEQ, DLEN, <DATA...>, CHK]
```

*   **SOP1**: `0xFF` (Start of Packet 1)
*   **SOP2**: `0xFF` (Start of Packet 2) - *Note: can be 0xFE for async, but we send 0xFF.*
*   **DID** (Device ID):
    *   `0x00`: Core (Ping, Version, etc.)
    *   `0x02`: Sphero (Roll, RGB, Back LED, etc.)
*   **CID** (Command ID): The specific action (e.g., `0x20` for Set RGB).
*   **SEQ**: Sequence number (0-255, rolling). Used to match ACKs.
*   **DLEN**: Data Length (Length of Data bytes + 1 for Checksum).
*   **DATA**: Payload bytes.
*   **CHK**: Checksum.

#### Checksum Algorithm
```cpp
uint8_t checksum = ~(sum(DID + CID + SEQ + DLEN + DATA...) % 256);
```

### Key Commands

| Command | DID | CID | Data Payload | Note |
| :--- | :--- | :--- | :--- | :--- |
| **Set RGB** | `0x02` | `0x20` | `[R, G, B, FLAG]` | `FLAG`: `0x00` (Temp), `0x01` (Persist). Use `0x00` for animations. |
| **Back LED** | `0x02` | `0x21` | `[BRIGHTNESS]` | `BRIGHTNESS`: 0-255. |
| **Ping** | `0x00` | `0x01` | `[]` | Used for Keep-Alive. |
| **Roll** | `0x02` | `0x30` | `[SPEED, HEAD_H, HEAD_L, STATE]` | `SPEED`: 0-255. `HEAD`: 0-359. |

## Technical Implementation Details

### 1. Write Types & Responsiveness
*   **Initialization**: Uses `ESP_GATT_WRITE_TYPE_RSP`. We must know the robot accepted the Anti-DOS key before sending the Wake command.
*   **Light Control**: Uses `ESP_GATT_WRITE_TYPE_NO_RSP`. Waiting for a BLE acknowledgement for every color change during a fade results in massive lag (1Hz updates). Using `NO_RSP` allows for smooth animations.

### 2. Rate Limiting & Synchronization
To prevent overwhelming the ESP32 BLE stack or the BB-8's internal buffer:
*   **Throttling**: The `loop()` function enforces a minimum **50ms** interval between packets when using `NO_RSP`.
*   **State Sync**: The component tracks `target_r` (from HA) vs `current_r` (sent to robot). If they differ, the `loop()` sends an update. This ensures that even if intermediate packets are dropped or throttled, the robot **always** eventually reaches the final requested color.
    *   *Insight*: On connection or startup, `current` values are initialized to `0xFE` (invalid) to force an immediate synchronization packet, ensuring the robot state matches Home Assistant even if the user hasn't touched the controls.

### 3. Flash Wear vs. Temporary State
The "Set RGB" command accepts a flag byte.
*   **`0x01` (Persist)**: Saves the color to the robot's non-volatile memory. **Avoid** using this for automation/fading. It is slow and wears out the flash.
*   **`0x00` (Temporary)**: Sets the color in RAM. This is fast and safe for rapid updates. This component uses `0x00` for all RGB commands.

### 4. Keep-Alive
If no commands are sent for 2 seconds, the robot may sleep or disconnect. The component sends a **Ping** packet (`DID 0x00, CID 0x01`) every 2 seconds if the command queue is idle.

## How to Extend

### Adding New Commands (e.g., Roll)
1.  **Define the Method**: Add a method to `SpheroBB8` (e.g., `void roll(uint8_t speed, uint16_t heading)`).
2.  **Implement Packet**: Use `send_packet` with `DID 0x02` and `CID 0x30`.
3.  **Expose to HA**: Create a `Template Output` or custom service in `__init__.py` to call this C++ method.

### Debugging
*   Enable `VERBOSE` logging in ESPHome to see raw packet dumps:
    ```yaml
    logger:
      level: VERBOSE
    ```
*   Look for `Sending packet DID=...` logs.
*   "Syncing RGB" logs indicate the internal loop is trying to catch up to the target state.

## References

*   **Gobot Sphero Driver**: The primary reference for the initialization sequence and packet structure.
    *   [sphero_driver.go](https://github.com/hybridgroup/gobot/blob/master/platforms/sphero/sphero_driver.go)
    *   [sphero_ollie_driver.go](https://github.com/hybridgroup/gobot/blob/master/drivers/ble/sphero/sphero_ollie_driver.go)
*   **SpheroV2.py**: Python implementation for newer toys, useful for comparing UUIDs.
    *   [spherov2.py](https://github.com/jchadwhite/spherov2.py)
*   **Unofficial API Docs**:
    *   [Sphero API Wiki](https://github.com/orbotix/Developer/wiki/Sphero-API-Packet-Structures)
