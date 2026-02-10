# Sphero BB-8 ESPHome Controller

WORK IN PROGRESS

Bring your iconic Sphero BB-8 droid into your smart home ecosystem with this custom ESPHome component. This project allows an ESP32 to act as a dedicated bridge, translating the proprietary Sphero BLE protocol into native Home Assistant entities. Once connected, your BB-8's lighting becomes a first-class citizen in your home automation, allowing you to incorporate its vibrant internal RGB LED and signature blue taillight into scenes, notifications, or manual controls alongside your other smart devices.

Whether you're using BB-8 as a unique status indicator or just want to see it glow as part of your "Movie Night" scene, this integration provides a seamless way to interact with your droid through the power of ESPHome and Home Assistant.

## Installation

To use this component in your ESPHome project, add the following to your configuration YAML. This will pull the latest version directly from the repository.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/scross01/bb8-esphome
    components: [ sphero_bb8 ]
```

## Configuration

Setting up your BB-8 requires finding its Bluetooth MAC address and defining the platforms. Below is a complete example configuration:

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf

# Required for Bluetooth communication
esp32_ble_tracker:

ble_client:
  - mac_address: D0:0E:71:8D:94:C7  # Replace with your BB-8's MAC address
    id: bb8_client
    auto_connect: false           # Crucial: prevents waking droid on ESP32 boot

sphero_bb8:
  id: bb8_hub
  ble_client_id: bb8_client
  auto_connect: false

light:
  - platform: sphero_bb8
    id: bb8_main_led
    name: "BB-8 Main LED"
    sphero_bb8_id: bb8_hub
    type: RGB

  - platform: sphero_bb8
    id: bb8_tail_led
    name: "BB-8 Tail Light"
    sphero_bb8_id: bb8_hub
    type: TAILLIGHT

button:
  - platform: sphero_bb8
    name: "BB-8 Connect"
    sphero_bb8_id: bb8_hub
    type: CONNECT

  - platform: sphero_bb8
    name: "BB-8 Disconnect"
    sphero_bb8_id: bb8_hub
    type: DISCONNECT

text_sensor:
  - platform: sphero_bb8
    name: "BB-8 Connection Status"
    sphero_bb8_id: bb8_hub
```

### Finding the MAC Address
If you are on Linux, you can easily find your droid's MAC address using `bluetoothctl`:
1. Run `bluetoothctl` in your terminal.
2. Type `scan on` and look for a device named "BB-8" followed by a series of numbers and letters.
3. Note the MAC address (e.g., `D0:0E:71:8D:94:C7`).
4. Type `quit` to exit.

## Configuration Variables

### sphero_bb8
- **id** (Required, ID): The ID to use for this hub.
- **ble_client_id** (Required, ID): The ID of the `ble_client` that connects to the BB-8.
- **auto_connect** (Optional, boolean): Whether to automatically connect to the BB-8 on startup. Defaults to `false`.

### light
- **platform** (Required, string): Must be `sphero_bb8`.
- **type** (Required, string): Either `RGB` for the main body LED or `TAILLIGHT` for the back LED.
- **sphero_bb8_id** (Required, ID): The ID of the `sphero_bb8` hub.
- **default_transition_length** (Optional, time): The duration of the color/brightness fade. Defaults to `1s`.
- All other options from [ESPHome Light](https://esphome.io/components/light/index.html).

### button
- **platform** (Required, string): Must be `sphero_bb8`.
- **name** (Required, string): The name of the button.
- **type** (Required, string): Either `CONNECT` or `DISCONNECT`.
- **sphero_bb8_id** (Required, ID): The ID of the `sphero_bb8` hub.
- All other options from [ESPHome Button](https://esphome.io/components/button/index.html).

### text_sensor
- **platform** (Required, string): Must be `sphero_bb8`.
- **name** (Required, string): The name of the status sensor.
- **sphero_bb8_id** (Required, ID): The ID of the `sphero_bb8` hub.
- All other options from [ESPHome Text Sensor](https://esphome.io/components/text_sensor/index.html).

## Technical Details

This component is designed specifically for the Sphero BB-8 and the ESP32. It utilizes the ESP-IDF framework to manage GATT operations and characteristic subscriptions. The implementation features a state-synchronization loop that ensures the droid reaches the desired color or brightness even during rapid transitions, while a built-in keep-alive mechanism maintains the connection during idle periods. The status sensor reports "Disconnected", "Connecting", "Initializing", "Ready", and "Disabling".
