import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID
from . import sphero_bb8_ns, SpheroBB8

CONF_SPHERO_BB8_ID = "sphero_bb8_id"
CONF_SCRIPT = "script"

ExecuteScriptAction = sphero_bb8_ns.class_("ExecuteScriptAction", automation.Action)

ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
    cv.Required(CONF_SCRIPT): cv.string,
})

@automation.register_action(
    "sphero_bb8.execute_script", ExecuteScriptAction, ACTION_SCHEMA
)
async def execute_script_to_code(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    var = cg.new_Pvariable(action_id, template_args)
    cg.add(var.set_parent(paren))
    cg.add(var.set_script_name(config[CONF_SCRIPT]))
    return var