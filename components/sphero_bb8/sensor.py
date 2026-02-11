import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_BATTERY_LEVEL,
    UNIT_PERCENT,
    DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT,
    CONF_ENTITY_CATEGORY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import sphero_bb8_ns, SpheroBB8, CONF_SPHERO_BB8_ID

DEPENDENCIES = ["sphero_bb8"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional("collision_speed"): sensor.sensor_schema(
            icon="mdi:speedometer",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("collision_magnitude"): sensor.sensor_schema(
            icon="mdi:pulse",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])

    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(parent.set_battery_sensor(sens))
        
    if "collision_speed" in config:
        sens = await sensor.new_sensor(config["collision_speed"])
        cg.add(parent.set_collision_speed_sensor(sens))
        
    if "collision_magnitude" in config:
        sens = await sensor.new_sensor(config["collision_magnitude"])
        cg.add(parent.set_collision_magnitude_sensor(sens))
