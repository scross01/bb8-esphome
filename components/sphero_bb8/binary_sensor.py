import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_NAME
from . import sphero_bb8_ns, SpheroBB8, CONF_SPHERO_BB8_ID

DEPENDENCIES = ["sphero_bb8"]

CONF_COLLISION = "collision"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    cg.add(parent.set_collision_sensor(var))
