# Sphero BB8 Controller for ESPHome

## Goal

Using ~/Development/gobot and ~/Development/bb8-gobot for referece, which is a Go Lang based implementation with support for controlling a sphero bb8 over BLE, the goal is to create a ESPHome library that can be can used to control a Sphero BB8 from a ESP32 device using Home Assistant.

The initial focus is to create a new platform for the bb8 device, providing the mac address in the configuration, and add support for light controllers for the bb8 lights.

e.g. ESPHome YAML
```yaml
sphero-bb8:
  mac: D0:0E:71:8D:94:C7

light:
  - platform: sphero-bb8
    id: rgb
    name: "Main LED"

  - platform: sphero-bb8
    id: taillight
    name: "Taillight LED"
```

The bb8-gotbot/ example works on this device running Ubuntu 25.10, so if we can mimic the same logic as a ESP32 library that will be a success.

## Guidelines

Reimplement the core controller functioanlity needed to connect and communicate with the bb8 device as a ESP32 C++ library for the ESP-IDF framework. 

Keep the implementation as light weight as possible, focus only on the functions needed for the BB8, do not expand to other sphero devices.

Include appropriate logging at info, debug and verbose levels to aid with debuggng the implementation.

## Reference Links:

- [SpheroV2 Python Library](https://spherov2.readthedocs.io/en/latest/toys.html#bb-8)

- [Sphero API](https://sdk.sphero.com/documentation/api-documents)

- gobot sphero drivers `/home/stephen/Development/gobot/drivers/ble/sphero`

