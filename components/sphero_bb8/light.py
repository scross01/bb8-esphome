import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID, CONF_ID, CONF_TYPE
from . import sphero_bb8_ns, SpheroBB8

DEPENDENCIES = ["sphero_bb8"]

SpheroBB8Light = sphero_bb8_ns.class_("SpheroBB8Light", light.LightOutput, cg.Component)

CONF_SPHERO_BB8_ID = "sphero_bb8_id"

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpheroBB8Light),
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Required(CONF_TYPE): cv.one_of("RGB", "TAILLIGHT", upper=True),
        cv.Optional(light.CONF_DEFAULT_TRANSITION_LENGTH, default="1s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
    
    await light.register_light(var, config)
