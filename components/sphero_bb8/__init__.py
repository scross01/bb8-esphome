import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

AUTO_LOAD = ["light", "button", "text_sensor", "sensor", "binary_sensor"]

sphero_bb8_ns = cg.esphome_ns.namespace("sphero_bb8")
SpheroBB8 = sphero_bb8_ns.class_("SpheroBB8", cg.Component, ble_client.BLEClientNode)

# Import the script types
Script = sphero_bb8_ns.struct("Script")
ScriptAction = sphero_bb8_ns.struct("ScriptAction")
ScriptActionType = sphero_bb8_ns.enum("ScriptActionType")

CONF_SPHERO_BB8_ID = "sphero_bb8_id"
CONF_AUTO_CONNECT = "auto_connect"
CONF_SCRIPTS = "scripts"
CONF_NAME = "name"
CONF_STEPS = "steps"

# Initialization overrides
CONF_WAKE_WAIT_MS = "wake_wait_ms"
CONF_STABILIZE_WAIT_MS = "stabilize_wait_ms"
CONF_RESTORE_LIGHTS_ON_CONNECT = "restore_lights_on_connect"

# Script action configurations
CONF_ACTION_ROLL = "roll"
CONF_ACTION_STOP = "stop"
CONF_ACTION_SPIN = "spin"
CONF_ACTION_RAW_MOTOR = "raw_motor"
CONF_ACTION_SET_HEADING = "set_heading"
CONF_ACTION_LIGHT = "light"
CONF_ACTION_BACK_LIGHT = "back_light"
CONF_ACTION_CENTER_HEAD = "center_head"
CONF_ACTION_STABILIZE = "stabilize"
CONF_ACTION_WAIT = "wait"
CONF_ACTION_REPEAT = "repeat"

# Roll action parameters
CONF_HEADING = "heading"
CONF_SPEED = "speed"

# Raw motor action parameters
CONF_LEFT_SPEED = "left_speed"
CONF_RIGHT_SPEED = "right_speed"

# Light action parameters
CONF_RED = "red"
CONF_GREEN = "green"
CONF_BLUE = "blue"

# Back light action parameters
CONF_BRIGHTNESS = "brightness"

# Stabilize action parameters
CONF_STATE = "state"

# Spin action parameters
CONF_RATE = "rate"

# Wait action parameters
CONF_DURATION = "duration"

# Repeat action parameters
CONF_COUNT = "count"

# Define individual action schemas first
ACTION_ROLL_SCHEMA = cv.Schema({
    cv.Required(CONF_HEADING): cv.int_range(min=0, max=359),
    cv.Required(CONF_SPEED): cv.int_range(min=0, max=255),
})

ACTION_SPIN_SCHEMA = cv.Schema({
    cv.Required(CONF_RATE): cv.int_range(min=0, max=255),
})

ACTION_RAW_MOTOR_SCHEMA = cv.Schema({
    cv.Required(CONF_LEFT_SPEED): cv.int_range(min=-255, max=255),
    cv.Required(CONF_RIGHT_SPEED): cv.int_range(min=-255, max=255),
})

ACTION_SET_HEADING_SCHEMA = cv.Schema({
    cv.Optional(CONF_HEADING, default=0): cv.int_range(min=0, max=359),
})

ACTION_LIGHT_SCHEMA = cv.Schema({
    cv.Required(CONF_RED): cv.int_range(min=0, max=255),
    cv.Required(CONF_GREEN): cv.int_range(min=0, max=255),
    cv.Required(CONF_BLUE): cv.int_range(min=0, max=255),
})

ACTION_BACK_LIGHT_SCHEMA = cv.Schema({
    cv.Required(CONF_BRIGHTNESS): cv.int_range(min=0, max=255),
})

ACTION_STABILIZE_SCHEMA = cv.Schema({
    cv.Required(CONF_STATE): cv.boolean,
})

# For wait action, we want to support both wait: 1s and wait: { duration: 1s }
ACTION_WAIT_SCHEMA = cv.Any(
    cv.positive_time_period_milliseconds,
    cv.Schema({
        cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
    })
)

# Forward declaration for recursive schema - will be updated after SCRIPT_ACTION_SCHEMA is defined
def validate_script_step(step):
    """Validate a script step - defined after SCRIPT_ACTION_SCHEMA."""
    if isinstance(step, dict):
        return validate_script_action(step)
    else:
        raise cv.Invalid("Script step must be an action dictionary")

ACTION_REPEAT_SCHEMA = cv.Schema({
    cv.Required(CONF_COUNT): cv.int_range(min=1, max=255),
    cv.Required(CONF_STEPS): cv.All([validate_script_step]),
})

SCRIPT_ACTION_SCHEMA = cv.Schema({
    cv.Optional(CONF_ACTION_ROLL): ACTION_ROLL_SCHEMA,
    cv.Optional(CONF_ACTION_STOP): cv.Schema({}),
    cv.Optional(CONF_ACTION_SPIN): ACTION_SPIN_SCHEMA,
    cv.Optional(CONF_ACTION_RAW_MOTOR): ACTION_RAW_MOTOR_SCHEMA,
    cv.Optional(CONF_ACTION_SET_HEADING): ACTION_SET_HEADING_SCHEMA,
    cv.Optional(CONF_ACTION_LIGHT): ACTION_LIGHT_SCHEMA,
    cv.Optional(CONF_ACTION_BACK_LIGHT): ACTION_BACK_LIGHT_SCHEMA,
    cv.Optional(CONF_ACTION_CENTER_HEAD): cv.Schema({}),
    cv.Optional(CONF_ACTION_STABILIZE): ACTION_STABILIZE_SCHEMA,
    cv.Optional(CONF_ACTION_WAIT): ACTION_WAIT_SCHEMA,
    cv.Optional(CONF_ACTION_REPEAT): ACTION_REPEAT_SCHEMA,
})

def validate_script_action(value):
    """Validate a script action."""
    if not isinstance(value, dict):
        raise cv.Invalid("Script action must be a dictionary")
    
    # Check for exactly one action type
    action_types = [
        CONF_ACTION_ROLL, CONF_ACTION_STOP, CONF_ACTION_SPIN, 
        CONF_ACTION_RAW_MOTOR, CONF_ACTION_SET_HEADING, CONF_ACTION_LIGHT,
        CONF_ACTION_BACK_LIGHT, CONF_ACTION_CENTER_HEAD, CONF_ACTION_STABILIZE,
        CONF_ACTION_WAIT, CONF_ACTION_REPEAT
    ]
    
    found_actions = []
    for action_type in action_types:
        if action_type in value:
            found_actions.append(action_type)
    
    if len(found_actions) != 1:
        raise cv.Invalid(f"Exactly one action type must be specified. Found: {found_actions}")
    
    # Apply the schema to validate and convert values (e.g., "150ms" to TimePeriod)
    return SCRIPT_ACTION_SCHEMA(value)

SCRIPT_SCHEMA = cv.Schema({
    cv.Required(CONF_NAME): cv.string,
    cv.Required(CONF_STEPS): cv.All([validate_script_step]),
})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SpheroBB8),
            cv.Optional(CONF_AUTO_CONNECT, default=False): cv.boolean,
            cv.Optional(CONF_WAKE_WAIT_MS, default=200): cv.int_range(min=0, max=5000),
            cv.Optional(CONF_STABILIZE_WAIT_MS, default=500): cv.int_range(min=0, max=10000),
            cv.Optional(CONF_RESTORE_LIGHTS_ON_CONNECT, default=True): cv.boolean,
            cv.Optional(CONF_SCRIPTS): cv.All([SCRIPT_SCHEMA]),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

async def generate_script_action(config, action_config):
    """Generate code for a script action."""
    # Add debugging to see what we're getting
    if not isinstance(action_config, dict):
        raise cv.Invalid(f"Expected action_config to be a dictionary, but got {type(action_config)}: {action_config}")
    
    for action_type, params in action_config.items():
        if action_type == CONF_ACTION_ROLL:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("roll_speed", params[CONF_SPEED]),
                ("roll_heading", params[CONF_HEADING]),
            )
        elif action_type == CONF_ACTION_STOP:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
            )
        elif action_type == CONF_ACTION_SPIN:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("spin_rate", params[CONF_RATE]),
            )
        elif action_type == CONF_ACTION_RAW_MOTOR:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("left_speed", params[CONF_LEFT_SPEED]),
                ("right_speed", params[CONF_RIGHT_SPEED]),
            )
        elif action_type == CONF_ACTION_SET_HEADING:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("heading", params.get(CONF_HEADING, 0)),
            )
        elif action_type == CONF_ACTION_LIGHT:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("red", params[CONF_RED]),
                ("green", params[CONF_GREEN]),
                ("blue", params[CONF_BLUE]),
            )
        elif action_type == CONF_ACTION_BACK_LIGHT:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("brightness", params[CONF_BRIGHTNESS]),
            )
        elif action_type == CONF_ACTION_CENTER_HEAD:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
            )
        elif action_type == CONF_ACTION_STABILIZE:
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("stabilize_state", params[CONF_STATE]),
            )
        elif action_type == CONF_ACTION_WAIT:
            # Handle both formats: wait: 1s and wait: { duration: 1s }
            if isinstance(params, dict):
                # Format: wait: { duration: 1s }
                duration_ms = params[CONF_DURATION]
            else:
                # Format: wait: 1s (params is the time period directly)
                duration_ms = params
            
            # ESPHome's cv.positive_time_period_milliseconds returns a TimePeriod object
            # with a .total_milliseconds property
            if hasattr(duration_ms, 'total_milliseconds'):
                # It's a TimePeriod object from ESPHome
                duration_ms = duration_ms.total_milliseconds
            elif isinstance(duration_ms, (int, float)):
                # Already in the correct format (milliseconds)
                duration_ms = int(duration_ms)
            else:
                # Fallback: try to convert to int assuming it's already in milliseconds
                try:
                    duration_ms = int(duration_ms)
                except (ValueError, TypeError):
                    # If all else fails, default to 0
                    duration_ms = 0
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("duration_ms", duration_ms),
            )
        elif action_type == CONF_ACTION_REPEAT:
            # Create the repeat action with nested steps
            repeat_steps = []
            for step in params[CONF_STEPS]:
                # Ensure step is a dictionary before passing it to generate_script_action
                if not isinstance(step, dict):
                    raise cv.Invalid(f"Expected step to be a dictionary, but got {type(step)}: {step}")
                repeat_steps.append(await generate_script_action(config, step))
            
            return cg.StructInitializer(
                ScriptAction,
                ("type", cg.RawExpression(f"sphero_bb8::ScriptActionType::{action_type.upper()}")),
                ("repeat_count", params[CONF_COUNT]),
                ("repeat_steps", repeat_steps),
            )

async def generate_script(config, script_config):
    """Generate code for a script."""
    steps = []
    for step in script_config[CONF_STEPS]:
        steps.append(await generate_script_action(config, step))
    
    return cg.StructInitializer(
        Script,
        ("name", script_config[CONF_NAME]),
        ("steps", steps),
    )

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_auto_connect(config[CONF_AUTO_CONNECT]))
    cg.add(var.set_wake_wait_ms(config[CONF_WAKE_WAIT_MS]))
    cg.add(var.set_stabilize_wait_ms(config[CONF_STABILIZE_WAIT_MS]))
    cg.add(var.set_restore_lights_on_connect(config[CONF_RESTORE_LIGHTS_ON_CONNECT]))
    
    # Add scripts if they are defined
    if CONF_SCRIPTS in config:
        for script_config in config[CONF_SCRIPTS]:
            script_struct = await generate_script(config, script_config)
            cg.add(var.add_script(script_struct))

# Import the action module to register the automation action
import esphome.components.sphero_bb8.action
