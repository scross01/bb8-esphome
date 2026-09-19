# Project Roadmap: Sphero BB-8 ESPHome Integration

This roadmap outlines the planned and potential features for the Sphero BB-8 ESPHome component, categorized by functional area.

## 1. Motion & Driving Control
Extend the component to allow full mobility of the BB-8 droid.

*   **`roll` Command**: Implement the core driving command (Speed, Heading).
*   **Driving Platform**: Create a `number` or `fan` platform to control speed and a `number` or `select` for heading.
*   **Joystick Support**: Integration with Home Assistant's dashboard joysticks for intuitive driving.
*   **Calibration (Heading Reset)**: A button to set the current orientation as "forward" (0 degrees), typically accompanied by turning on the blue back LED for visual alignment.
*   **Rotation Rate Control**: A slider to adjust how fast the droid turns.
*   **Stabilization Toggle**: A switch to enable/disable the internal gyroscopic stabilization (useful for specific tricks or troubleshooting).

## 2. Head & Neck Interaction (Carriage Control)
Since BB-8 lacks dedicated neck motors (unlike R2-D2), head movement is achieved by manipulating the internal carriage.

*   **Head Rotation**: Implement a `number` or `select` platform using the `Set Heading` command (`DID 0x02, CID 0x01`). When speed is 0, this rotates the carriage and spins the magnetic head.
*   **Head Tilt/Nod**: Use the `Roll` command (`DID 0x02, CID 0x30`) with 0 speed and short heading changes to "lean" the head.
*   **Animations (Shake/Jitter)**: Use `Set Raw Motors` (`DID 0x02, CID 0x33`) to rapidly oscillate the internal wheels, causing the head to wobble or shake without moving the body shell.
*   **Center Head**: Implement a `button` to trigger `Self Level` (`DID 0x02, CID 0x09`) to snap the head back to the top position.

## 3. Sensors & Environmental Feedback
Leverage the BB-8's internal sensors to provide real-time data to Home Assistant.

*   **Battery Management**: 
    *   **Battery Level Sensor**: Report percentage/voltage using the `GetPowerState` command.
    *   **Charging Status**: Indicate if the droid is on its inductive base.
*   **Collision Detection**:
    *   **Collision Binary Sensor**: Trigger an event in HA when the droid bumps into something.
    *   **Collision Impact Vector**: Report the direction and magnitude of hits.
*   **IMU Data (Motion Tracking)**:
    *   **Accelerometer/Gyro Sensors**: Real-time X, Y, Z motion data.
    *   **Orientation (Pitch/Roll/Yaw)**: Report the droid's physical tilt and rotation.
*   **Odometer/Locator**: Track the droid's (X, Y) position relative to its starting point.

## 3. Diagnostics & Maintenance
Improve the visibility of the connection and internal droid health.

*   **Bluetooth Signal (RSSI)**: A sensor to monitor the BLE signal strength.
*   **Connection Stability Metrics**: Track packet success rate and latency.
*   **Error Reporting**: Expose internal droid error codes (e.g., motor stall, sensor failure).
*   **Keep-Alive Tuning**: Configurable intervals for the Ping mechanism.
*   **Firmware Version**: A text sensor reporting the droid's internal firmware version.

## 4. Automation & Interaction Features
Enable more complex "personality" behaviors for the droid.

*   **Pre-defined Animations**: Trigger built-in Sphero "macros" or animations (e.g., "Yes", "No", "Spin").
*   **Sound Integration**: (Note: BB-8 sounds are usually played through the original app; we could trigger Home Assistant sounds in sync with droid movements).
*   **Collision Responses**: Automated "shake" or "color flash" logic handled directly on the ESP32 for lower latency.

## 5. Implementation Key Insights (Technical)

*   **Data Streaming**: The BB-8 supports asynchronous data streaming. We need to implement the `handleResponses` logic from Gobot to parse the 88-byte sensor packets.
*   **Command Acknowledgments**: Many commands return an ACK packet. We can use these to update "Last Command Status" sensors.
*   **Inductive Charging**: The droid can be woken up or put to sleep based on its presence on the charging base, which can be detected via the power state packet.

## 6. Macros

Can you upload new macros?
  Yes, technically. The Sphero V1 protocol (which the BB-8 uses) supports a command group (DID 0x08) for
  Macro Management. This includes:
   * Save Temporary Macro (`CID 0x01`): Uploads a macro to RAM for immediate execution.
   * Save Persistent Macro (`CID 0x03`): Saves a macro to the droid's non-volatile memory.
   * Run Macro (`CID 0x02`): Executes a macro by its ID.


  The "Predefined" Set
  The "Yes," "No," "Angry," and "Giddy" animations you mentioned are actually System Macros (IDs 0-31)
  that are pre-baked into the firmware by Sphero. These are what most apps trigger when they want a
  "canned" animation.


  Current Implementation Status
  While the hardware supports it, the ESPHome component currently does not use the on-device macro
  system. Instead:
   1. Client-Side Scripting: The ScriptExecutor I just implemented runs the logic on the ESP32. It sends
      individual BLE packets (Roll, Light, etc.) to the BB-8 one by one.
   2. Why?: Uploading binary macros is extremely complex (it requires a custom bytecode format) and can
      easily "brick" the droid's responsiveness if a loop is written incorrectly.
   3. Recommendation: For now, use the scripts: block in your YAML to create "new" animations. It
      provides the same result (complex sequences) but is much easier to debug and modify without
      needing to manage the droid's internal memory.


  Summary: The hardware allows uploading new ones, but our component uses a "Scripting" approach on the
  ESP32 to achieve the same goal with more flexibility and safety.

What are all of the 32 system macros?
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
✦ In the Sphero V1 protocol (which BB-8 uses), "System Macros" are hardcoded routines stored in the
  firmware. While the protocol reserves IDs 1 through 31 for these macros, many are legacy holdovers
  from the original Sphero 1.0/2.0 and are either empty or non-functional on the BB-8.

  However, based on the Sphero SDK and reverse-engineering of the BB-8 firmware, here is the functional
  list of System Macros and Animations.


  1. Legacy System Macros (Firmware Defined)
  These are the standard "building block" macros inherited from the original Sphero API.



  ┌────┬──────────────┬─────────────────────────────────────────────────────────┐
  │ ID │ Name         │ Description                                             │
  ├────┼──────────────┼─────────────────────────────────────────────────────────┤
  │ 1  │ Fade         │ Gradually cycles the Main LED through colors.           │
  │ 2  │ Flash        │ A quick strobe of the Main LED.                         │
  │ 3  │ Double Flash │ Two quick pulses of light.                              │
  │ 4  │ Rainbow      │ A faster, multi-color cycling effect.                   │
  │ 5  │ Figure 8     │ Droid drives in a figure-8 pattern.                     │
  │ 6  │ Square       │ Droid drives in a square pattern.                       │
  │ 7  │ Circle       │ Droid drives in a continuous circle.                    │
  │ 8  │ Spin         │ Rotates the internal carriage rapidly (spins the head). │
  │ 9  │ Wave         │ A back-and-forth oscillation.                           │
  └────┴──────────────┴─────────────────────────────────────────────────────────┘

  Note: IDs 10–31 are largely empty placeholders in the BB-8 firmware or are used for factory testing.

  ---


  2. BB-8 "Personality" Animations
  When users talk about "Built-in Animations" for BB-8 (the ones seen in the Star Wars app), they are
  usually referring to Macro 255 (User Macro) slots that the app uploads on the fly, or specific
  combinations of commands.


  However, the BB-8 has a few "hidden" routines that replicate the behaviors from the movie:



  ┌────────────┬───────────────────────────────────────────────────────────────────────────────────────┐
  │ Animation  │ Behavior                                                                              │
  ├────────────┼───────────────────────────────────────────────────────────────────────────────────────┤
  │ Yes        │ A rapid forward-and-back pitch of the internal carriage, causing the head to nod.     │
  │ No         │ A side-to-side oscillation of the carriage (heading change), causing the head to sha… │
  │ Angry      │ High-frequency motor vibration (Raw Motor) while the LED flashes red.                 │
  │ Giddy      │ A "dancing" wobble created by rapidly shifting the center of mass.                    │
  │ Scared     │ Head shakes while the droid rapidly reverses a few inches.                            │
  │ **Look Ar… │ Random heading changes with 0 speed to spin the head in different directions.         │
  └────────────┴───────────────────────────────────────────────────────────────────────────────────────┘


---

## References
- [Spherov2.py Source](https://github.com/artificial-intelligence-upenn/spherov2.py): Best Python reference for the proprietary protocol (specifically `commands/sphero.py`).
- [BB8Toy.smali analysis](apktool/base/smali_classes2/com/sphero/android/convenience/autogen/toys/BB8Toy.smali): Confirms BB-8 uses V1 (`DID 0x02`) and lacks V2 `Animatronic` commands.
- [Gobot Sphero Driver](https://github.com/hybridgroup/gobot/tree/master/drivers/ble/sphero): Clean implementation of V1 packet encoding and checksums.
- [Unofficial Sphero Protocol Wiki](https://github.com/orbotix/Developer/wiki/Sphero-API-Packet-Structures): Excellent for packet frame and data type definitions.
