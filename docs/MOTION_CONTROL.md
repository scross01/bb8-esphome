# Proposal: BB-8 Motion Scripting Engine (V2)

This document outlines the design for enhancing the Sphero BB-8 ESPHome component with a custom motion scripting engine. This feature allows users to define complex, multi-step behaviors in YAML that replicate the "Blockly" programming experience of the official Sphero apps.

## 1. Objective
Enable users to program sequences of movements, lighting changes, and specialized commands (e.g., head rotations, patrol patterns) directly within the ESPHome configuration, exposing these sequences as triggerable actions in Home Assistant.

## 2. Script Actions

| Action | Parameters | Description |
| :--- | :--- | :--- |
| `roll` | `heading` (0-359), `speed` (0-255) | Moves at speed in a specific direction. |
| `stop` | - | Immediate deceleration. |
| `spin` | `rate` (0-255) | Sets how fast the robot rotates on its axis. |
| `raw_motor` | `left_speed`, `right_speed` | Direct voltage to motors (bypasses gyro). |
| `set_heading`| `heading` (default 0) | Sets the current orientation as the new 0° (Aim). |
| `light` | `red`, `green`, `blue` (0-255) | Updates the main RGB LED. |
| `back_light`| `brightness` (0-255) | Updates the blue tail (Aim) LED. |
| `center_head`| - | Triggers the `Self Level` command. |
| `stabilize` | `state` (bool) | Enables or disables the internal gyro. |
| `wait` | `duration` (time) | Pauses script execution. |
| `repeat` | `count` (int), `steps` (list) | Repeats a sub-sequence of commands. |

## 3. Proposed YAML Syntax
A new `scripts` block will be added to the `sphero_bb8` hub configuration.

```yaml
sphero_bb8:
  id: bb8_hub
  scripts:
    - name: "Head Shake"
      steps:
        - repeat:
            count: 3
            steps:
              - roll: { heading: 30, speed: 0 }
              - wait: 150ms
              - roll: { heading: 330, speed: 0 }
              - wait: 150ms
        - center_head: {}

    - name: "Patrol Circle"
      steps:
        - light: { red: 0, green: 255, blue: 0 }
        - roll: { heading: 0, speed: 100 }
        - wait: 1s
        - roll: { heading: 90, speed: 100 }
        - wait: 1s
        - roll: { heading: 180, speed: 100 }
        - wait: 1s
        - roll: { heading: 270, speed: 100 }
        - wait: 1s
        - stop: {}
        - light: { red: 0, green: 0, blue: 255 }
```

## 4. Architecture & Implementation

### C++ Script Engine
The `SpheroBB8` hub will be updated to include a `ScriptExecutor` module:
1.  **Storage:** Scripts will be stored as a `std::vector` of `CommandStep` structures.
2.  **Execution:** A non-blocking state machine in the `loop()` function will track the `current_step_index` and use `millis()` to handle transitions between steps.
3.  **Concurrency:** Manual commands sent from Home Assistant (e.g., a light change) will by default cancel any running script to ensure the user always has immediate control.

### Python Code Generation
The `__init__.py` will parse the `scripts` list and:
1.  Generate the C++ data structures for each script.
2.  Register a `button` entity for each script so it appears automatically in Home Assistant.
3.  Register a `sphero_bb8.execute_script` action.

## 5. Technical Considerations
*   **Packet Rate Limiting:** The script engine must adhere to the existing 50ms safety interval for BLE packets to avoid overwhelming the BB-8's MCU.
*   **Aiming Logic:** The `set_heading` command allows the script to define "Forward" based on the current orientation, which is essential for consistent movement.
*   **Memory Efficiency:** Since the ESP32 has limited memory, scripts will be processed into a compact binary format for the C++ core.

## 6. Implementation Decisions
*   **Interrupt Handling:** All manual commands will always interrupt running scripts to ensure user has immediate control.
*   **Error Handling:** Obstacles during movement will be ignored and the script will continue, but execution errors will abort the script.
*   **Concurrency:** Only one script runs at a time; new scripts will interrupt any currently running script.
*   **Parameter Validation:** Parameter validation occurs at YAML configuration time to ensure valid values.
*   **State Persistence:** Script execution stops if the ESP32 is restarted or the BB-8 connection is dropped.
*   **Battery Management:** No specific safeguards for battery consumption during script execution.
*   **Action Coordination:** Only sequential execution of actions is supported, not simultaneous actions.
*   **Timing Precision:** Timing operates on a best-effort basis considering system constraints.
*   **Memory Limits:** Memory usage depends on the ESP32 device and is not specifically controlled by the script engine.
*   **Debugging:** Informational logs will be output when a script starts and completes, with debug logs for each command sent.

## 7. Home Assistant Integration
Each script defined in YAML will be exposed as:
*   A **Button Entity** (e.g., `button.bb8_head_shake`).
*   A **Service Call** (`esphome.bb8_controller_execute_script`).

---
**Status:** Updated Plan (Audio interaction removed)