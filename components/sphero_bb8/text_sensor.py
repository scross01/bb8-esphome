import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_DIAGNOSTIC
from . import sphero_bb8_ns, SpheroBB8, CONF_SPHERO_BB8_ID

DEPENDENCIES = ["sphero_bb8"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
        cv.Optional("firmware_version"): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:chip",
        ),
        cv.Optional("charging_status"): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:battery-charging",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # This handles the default/single text_sensor if it's configured as a single entity
    # But usually, with platform schemas, we iterate.
    # However, the previous implementation assumed `text_sensor:` entry *was* the status sensor?
    # No, the previous implementation used `text_sensor.text_sensor_schema()`, implying the whole entry is ONE sensor.
    # To support multiple, we should probably check if specific keys are present, OR
    # simply use `text_sensor.new_text_sensor` for the main config if it's a list.
    # But existing config was:
    # text_sensor:
    #   - platform: sphero_bb8
    #     name: "BB-8 Connection Status"
    #
    # We want:
    # text_sensor:
    #   - platform: sphero_bb8
    #     name: "BB-8 Connection Status"
    #     firmware_version:
    #       name: "BB-8 Firmware"
    #     charging_status:
    #       name: "BB-8 Charging Status"
    
    # So we need to handle the main sensor (Connection Status) AND optional extra sensors.
    
    # The `text_sensor_schema()` creates a schema for a single sensor (name, id, etc).
    # If we extend it, the root of the config is that sensor.
    
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    cg.add(parent.set_status_sensor(var))

    if "firmware_version" in config:
        sens = await text_sensor.new_text_sensor(config["firmware_version"])
        cg.add(parent.set_version_sensor(sens))
        
    if "charging_status" in config:
        sens = await text_sensor.new_text_sensor(config["charging_status"])
        cg.add(parent.set_charging_status_sensor(sens))
