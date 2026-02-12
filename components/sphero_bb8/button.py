import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_CONFIG, CONF_TYPE
from . import sphero_bb8_ns, SpheroBB8

DEPENDENCIES = ["sphero_bb8"]

SpheroBB8Button = sphero_bb8_ns.class_("SpheroBB8Button", button.Button, cg.Component)
SpheroBB8ScriptButton = sphero_bb8_ns.class_("SpheroBB8ScriptButton", button.Button, cg.Component)

CONF_SPHERO_BB8_ID = "sphero_bb8_id"
CONF_SCRIPT_NAME = "script_name"

BUTTON_TYPES = ["CONNECT", "DISCONNECT", "CENTER_HEAD"]

SCRIPT_BUTTON_CONFIG_SCHEMA = button.button_schema(
    SpheroBB8ScriptButton, 
    entity_category=ENTITY_CATEGORY_CONFIG
).extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Required(CONF_SCRIPT_NAME): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

BUTTON_CONFIG_SCHEMA = button.button_schema(SpheroBB8Button).extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        cv.Required(CONF_TYPE): cv.one_of(*BUTTON_TYPES, upper=True),
        cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)

def validate_button_config(value):
    """Validate that the button configuration is valid."""
    if CONF_TYPE in value:
        # Regular button
        return BUTTON_CONFIG_SCHEMA(value)
    elif CONF_SCRIPT_NAME in value:
        # Script button
        return SCRIPT_BUTTON_CONFIG_SCHEMA(value)
    else:
        raise cv.Invalid("Either 'type' or 'script_name' must be specified for sphero_bb8 buttons")


CONFIG_SCHEMA = validate_button_config

async def to_code(config):
    if CONF_TYPE in config:
        # Regular button
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await button.register_button(var, config)

        parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
        cg.add(var.set_parent(parent))
        cg.add(var.set_type(config[CONF_TYPE]))
    elif CONF_SCRIPT_NAME in config:
        # Script button
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await button.register_button(var, config)

        parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
        cg.add(var.set_parent(parent))
        cg.add(var.set_script_name(config[CONF_SCRIPT_NAME]))
