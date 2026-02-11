# Development Guide & Architecture

This document provides detailed technical information about the internal workings of the Sphero BB-8 ESPHome component. It is intended for developers and AI agents who wish to extend, maintain, or debug the project.

## Architecture Overview

The project is built as a custom **C++ component** for ESPHome, leveraging the **ESP-IDF framework**. It acts as a bridge between Home Assistant (via ESPHome's API) and the Sphero BB-8 droid (via Bluetooth Low Energy).

### Component Structure

The component is split into four primary functional areas:

1.  **`SpheroBB8` (Hub)**:
    *   Inherits from `esphome::Component` and `esphome::ble_client::BLEClientNode`.
    *   Manages the BLE connection lifecycle and the `BLEClient` enabled/disabled state.
    *   Handles the complex initialization handshake (Subscribe, Anti-DOS, TX Power, Wake).
    *   Maintains a registration list of `SpheroBB8Light` entities to manage their states globally.
    *   Implements the raw Sphero API packet encoding and checksum logic.

2.  **`SpheroBB8Light` (Platform)**:
    *   Inherits from `esphome::light::LightOutput`.
    *   Translates ESPHome light state (RGB, Brightness) into Sphero commands.
    *   **UI Feedback Logic**: If a user attempts to toggle a light when the hub is not in the `READY` state, the light immediately uses `make_call()` to publish its state back to `OFF` in Home Assistant, providing immediate UI feedback that the droid is unavailable.

3.  **`SpheroBB8Button` (Platform)**:
    *   Inherits from `esphome::button::Button`.
    *   Triggers connection actions. `CONNECT` enables the parent `BLEClient` to initiate a link, while `DISCONNECT` triggers the `DISABLING` sequence.

4.  **`SpheroBB8 Connection Status` (Text Sensor)**:
    *   Reports the current state of the connection and initialization (e.g., "Disconnected", "Connecting", "Initializing", "Ready", "Disabling").

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
5.  **Stabilize**: Wait 1000ms in `READY_STABILIZE` state. *Ensures the droid's firmware is fully ready to process unacknowledged commands.*

*Note: All initialization writes should use `ESP_GATT_WRITE_TYPE_RSP` (Write with Response) to ensure sequential execution.*

### Packet Structure

Commands sent to the **Commands Characteristic** (`2ba1`) follow this binary structure:

```
[SOP1, SOP2, DID, CID, SEQ, DLEN, <DATA...>, CHK]
```

*   **SOP1**: `0xFF` (Start of Packet 1)
*   **SOP2**: `0xFF` (Start of Packet 2) - *Note: can be 0xFE for async, but we send 0xFF.*
*   **DID** (Device ID):
    *   `0x00`: Core (Ping, Version, Sleep, etc.)
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
| **Sleep** | `0x00` | `0x22` | `[0,0,0,0,0]` | Puts droid into low-power sleep mode. |
| **Roll** | `0x02` | `0x30` | `[SPEED, HEAD_H, HEAD_L, STATE]` | `SPEED`: 0-255. `HEAD`: 0-359. |
| **Get Power**| `0x00` | `0x20` | `[]` | Requests current power state (Charging, OK, Low, Critical). |
| **Set Pwr Notify**| `0x00` | `0x21` | `[ENABLE]` | `ENABLE`: `0x01` to subscribe to async power updates. |
| **Get Version**| `0x00` | `0x02` | `[]` | Requests version info. MSA Version (Main App) is parsed from response. |
| **Config Collision**| `0x02` | `0x12` | `[METH, Xt, Xs, Yt, Ys, DT]` | Configures collision detection service. |

### Sensors & Notifications

The component implements several sensors to report the droid's status:

1.  **Battery Level & Charging Status**:
    *   **Polling**: The hub polls the power state (`Get Power State`) every 60 seconds.
    *   **Asynchronous Updates**: On connection, the hub enables power notifications (`Set Power Notification`). This allows the droid to push updates immediately when charging starts/stops or battery level changes.
    *   **Packet Handling**: The `process_packet_` method detects asynchronous packets by checking for `SOP2 = 0xFE`. It parses the payload (State Code) to update the sensors:
        *   `0x01`: Charging (100%)
        *   `0x02`: OK (100%)
        *   `0x03`: Low (20%)
        *   `0x04`: Critical (5%)

2.  **Firmware Version**:
    *   Requested once, 3 seconds after the connection is established.
    *   The packet sequence number is tracked (`version_req_seq_`) to correctly identify the response amidst other traffic.
    *   The Main Application (MSA) version bytes are extracted from the payload indices 8 and 9.

3.  **Collision Detection**:
    *   **Configuration**: Upon connection, the component sends `CID_CONFIG_COLLISION` (`0x12`) to enable the service with default thresholds (100) and deadtime (500ms).
    *   **Async Notifications**: The droid sends an async packet with ID `0x07` upon impact.
    *   **Sensors**:
        *   **Binary Sensor**: Toggles to `True` on impact and auto-resets to `False` after 500ms.
        *   **Collision Speed**: Reports the impact speed (0-255).
        *   **Collision Magnitude**: Reports the vector magnitude ($\sqrt{x^2 + y^2}$) of the impact force.
    *   **Packet Buffer**: A rolling buffer (`packet_buffer_`) is used to reassemble split BLE notifications, ensuring robust parsing of the multi-byte collision payload.

## Technical Implementation Details

### 1. Write Types & Responsiveness
*   **Initialization**: Uses `ESP_GATT_WRITE_TYPE_RSP`. We must know the robot accepted the Anti-DOS key before sending the Wake command.
*   **Light Control**: Uses `ESP_GATT_WRITE_TYPE_NO_RSP`. Waiting for a BLE acknowledgement for every color change during a fade results in massive lag. Using `NO_RSP` allows for smooth animations.

### 2. Rate Limiting & Synchronization
To prevent overwhelming the ESP32 BLE stack or the BB-8's internal buffer:
*   **Throttling**: The `loop()` function enforces a minimum **50ms** interval between packets when using `NO_RSP`.
*   **State Sync**: The component tracks `target_r` (from HA) vs `current_r` (sent to robot). If they differ, the `loop()` sends an update. This ensures that even if intermediate packets are dropped or throttled, the robot **always** eventually reaches the final requested color.
    *   *Force Sync*: On connection or startup, `current` values are initialized to `0xFE` (invalid) to force an immediate synchronization packet.

### 3. Clean Disconnect Sequence
When the `DISCONNECT` button is pressed, the hub enters a `DISABLING` state.
1.  Sends a **Sleep** command to the droid (turns off lights and puts processor to sleep).
2.  Waits 500ms for the packet to clear the BLE stack.
3.  Disables the parent `BLEClient` component, closing the link.
4.  Calls `force_lights_off_()`, which iterates through all registered lights and publishes an `OFF` state to Home Assistant.

### 4. Keep-Alive
If no commands are sent for 2 seconds, the robot may sleep or disconnect. The component sends a **Ping** packet (`DID 0x00, CID 0x01`) every 2 seconds if the command queue is idle.

## How to Extend

### Adding New Commands (e.g., Roll)
1.  **Define the Method**: Add a method to `SpheroBB8` (e.g., `void roll(uint8_t speed, uint16_t heading)`).
2.  **Implement Packet**: Use `send_packet` with `DID 0x02` and `CID 0x30`.
3.  **Expose to HA**: Create a custom service or button in the Python configuration to trigger the C++ method.

### Debugging
*   Enable `VERBOSE` logging in ESPHome to see raw packet dumps:
    ```yaml
    logger:
      level: VERBOSE
    ```
*   Look for `Sending packet DID=...` logs.
*   "Syncing RGB" logs indicate the internal loop is trying to catch up to the target state.

## References

*   **Gobot Sphero Driver**: Primary reference for protocol and initialization.
    *   [sphero_driver.go](https://github.com/hybridgroup/gobot/blob/master/platforms/sphero/sphero_driver.go)
    *   [sphero_ollie_driver.go](https://github.com/hybridgroup/gobot/blob/master/drivers/ble/sphero/sphero_ollie_driver.go)
*   **SpheroV2.py**: Python implementation for newer toys.
    *   [spherov2.py](https://github.com/jchadwhite/spherov2.py)
*   **Unofficial API Docs**:
    *   [Sphero API Wiki](https://github.com/orbotix/Developer/wiki/Sphero-API-Packet-Structures)
