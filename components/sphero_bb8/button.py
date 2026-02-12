import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_CONFIG, CONF_TYPE
from . import sphero_bb8_ns, SpheroBB8

DEPENDENCIES = ["sphero_bb8"]

SpheroBB8Button = sphero_bb8_ns.class_("SpheroBB8Button", button.Button, cg.Component)

CONF_SPHERO_BB8_ID = "sphero_bb8_id"

CONFIG_SCHEMA = button.button_schema(SpheroBB8Button).extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Required(CONF_TYPE): cv.one_of("CONNECT", "DISCONNECT", "CENTER_HEAD", upper=True),
        cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
    
    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
