# Sphero BB-8 Protocol Reference

This document details the implementation and protocol specifics for interacting with the Sphero BB-8, based on the analysis of the Gobot framework source code found in the `reference/` directory.

## Key Source Files

The following files in the `reference/gobot` directory provide the most insight into the BB-8's operation:

*   **`reference/gobot/drivers/ble/sphero/sphero_bb8_driver.go`**:
    *   **Purpose**: The entry point for the BB-8 driver.
    *   **Insight**: Confirms that BB-8 shares the same driver logic as the Sphero Ollie. It initializes the driver with `NewOllieDriver`.

*   **`reference/gobot/drivers/ble/sphero/sphero_ollie_driver.go`**:
    *   **Purpose**: Contains the core logic for controlling the robot via BLE.
    *   **Insight**: Defines the BLE UUIDs, initialization sequence, command packet construction, and specific command implementations (Roll, SetRGB, etc.).

*   **`reference/gobot/drivers/common/spherocommon/spherocommon.go`**:
    *   **Purpose**: Shared utilities for Sphero robots.
    *   **Insight**: Contains the checksum calculation algorithm and common event string constants.

*   **`reference/gobot/drivers/common/spherocommon/spherocommon_packets.go`**:
    *   **Purpose**: Defines data structures for protocol packets.
    *   **Insight**: Details the structure of response packets like `CollisionPacket`, `PowerStatePacket`, and `DataStreamingPacket`.

## BLE Characteristics (UUIDs)

The BB-8 uses the following BLE characteristics for communication. Note that "Anti-DOS" is a security mechanism to prevent unauthorized access.

| Name | UUID | Usage |
| :--- | :--- | :--- |
| **Wake** | `22bb746f-2bbf-7554-2d6f-726568705327` | Write `0x01` to wake the device. |
| **TX Power** | `22bb746f-2bb2-7554-2d6f-726568705327` | Set transmission power. |
| **Anti-DOS** | `22bb746f-2bbd-7554-2d6f-726568705327` | Write specialized string to unlock. |
| **Commands** | `22bb746f-2ba1-7554-2d6f-726568705327` | Send command packets here. |
| **Response** | `22bb746f-2ba6-7554-2d6f-726568705327` | Subscribe to receive responses/async events. |

## Initialization Sequence

All four actions below must complete before sending any Sphero command packet — the droid will not accept commands (and may disconnect) otherwise. Two orderings exist in practice:

**This component's order** (implemented in `sphero_bb8.cpp`, verified on hardware):

1.  **Subscribe**: Enable notifications on the **Response** characteristic to receive feedback.
2.  **Anti-DOS**: Write the string `"011i3"` to the **Anti-DOS** characteristic.
3.  **Set TX Power**: Write `0x07` (byte) to the **TX Power** characteristic.
4.  **Wake**: Write `0x01` (byte) to the **Wake** characteristic.
5.  **Stabilize**: Wait `stabilize_wait_ms` (default 500ms) before treating the droid as READY.

**Gobot reference order** (`reference/gobot/drivers/ble/sphero/sphero_ollie_driver.go`, `initialize()`): Anti-DOS → Set TX Power → Wake → Subscribe (subscription last).

Neither ordering is "the required one" — subscribing first (component order) ensures no early async packets are missed, while gobot subscribes last and also works. The requirement is only that all four actions complete before the first command packet.

## Packet Structure

Commands sent to the **Commands** characteristic follow a specific binary format:

`[SOP1, SOP2, DID, CID, SEQ, DLEN, <DATA>, CHK]`

*   **SOP1**: Start of Packet 1 (`0xFF`)
*   **SOP2**: Start of Packet 2 (`0xFF`)
*   **DID**: Device ID (Target device for the command)
*   **CID**: Command ID (Specific action to perform)
*   **SEQ**: Sequence Number (Incrementing byte)
*   **DLEN**: Data Length (Length of `<DATA>` + 1 for Checksum)
*   **DATA**: Payload bytes (Arguments for the command)
*   **CHK**: Checksum

### Checksum Algorithm

The checksum is calculated by summing all bytes from `DID` through the last `DATA` byte — **including `DLEN`, excluding the `SOP1`/`SOP2` bytes and the checksum itself** — taking the modulo 256, and inverting the bits. In the gobot source this is the `buf[2:]` slice passed to `CalculateChecksum` in `craftPacket()`.

```go
// Go implementation from spherocommon.go
func CalculateChecksum(buf []byte) byte {
    var calculatedChecksum uint16
    for i := range buf {
        calculatedChecksum += uint16(buf[i])
    }
    return uint8(^(calculatedChecksum % 256))
}
```

## Common Commands

| Command | DID | CID | Data Payload | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Set RGB** | `0x02` | `0x20` | `[R, G, B]` | 3-byte form per the official app's `SetMainLedCommand.toByteList(SSS)` (APK decode). Legacy 4-byte variants `[R,G,B,flag]` are also accepted by the firmware (gobot sends flag=0x01); the trailing byte is ignored. |
| **Back LED** | `0x02` | `0x21` | `[Brightness]` | 0-255 brightness. |
| **Roll** | `0x02` | `0x30` | `[Speed, Heading_H, Heading_L, RollMode]` | `RollMode` enum per APK: 0=Stop, 1=Go, 2=Calibrate. The official app additionally appends a 5th byte, `ReverseFlag` (0=Off, 1=On); its absence is treated as Off. |
| **Stop** | `0x02` | `0x30` | `[0, 0, 0, 0x01]` | `Roll(0, GO)`: speed 0 with RollMode=Go (decelerate to a stop while keeping heading control). The APK's `RollMode=Stop` (0) is a distinct, harder stop — not used here. |
| **Raw Motor** | `0x02` | `0x33` | `[LMode, LPower, RMode, RPower]` | Modes: 0=Off, 1=Fwd, 2=Rev, 3=Brake. |
| **Sleep** | `0x00` | `0x22` | `[0, 0, 0, 0, 0]` | Deep sleep. |
| **Ping** | `0x00` | `0x01` | `[]` | Keep-alive (implied). |
| **Center Head (Self Level)** | `0x02` | `0x09` | `[Options, AngleLimit, Timeout, TrueTime]` | `Options` is a bitmask: bit0=Start (`0x01`), bit1=Level (`0x02`), bit2=Control (`0x04`), bit3=LockHeading (`0x08`); the remaining bytes are angle-limit, timeout, and true-time. The component sends `[0x01, 0x00, 0x00, 0x00]` (start with defaults). Layout per `SelfLevelCommand.toByteList` in the decompiled APK (`reference/apktool/`); field names per the Sphero V2 Python API signature `self_level(opt1..opt4, angle_limit, timeout, true_time)` (`reference/spherov2-readthedocs-io-en-latest.md`). |

When sources disagree on a payload layout, the decompiled Android app is the primary
reference (`reference/apktool/.../autogen/commands/sphero/`), then the gobot driver,
then the spherov2 documentation.

## Asynchronous Responses

The BB-8 sends responses and events via the **Response** characteristic.

*   **Header**: `[0xFF, 0xFF]` implies a synchronous response.
*   **Header**: `[0xFF, 0xFE]` implies an asynchronous event.

### Collision Detection
*   **Code**: `0x07`
*   **Payload**: Contains impact axis, magnitude, and speed. Configuration is required via CID `0x12` (Configure Collision Detection).
