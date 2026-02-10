import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_DIAGNOSTIC
from . import sphero_bb8_ns, SpheroBB8

DEPENDENCIES = ["sphero_bb8"]

CONF_SPHERO_BB8_ID = "sphero_bb8_id"

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Optional(CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    
    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    cg.add(parent.set_status_sensor(var))
