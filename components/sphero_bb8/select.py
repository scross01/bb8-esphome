import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
from esphome.core import CORE
from . import sphero_bb8_ns, SpheroBB8, CONF_SCRIPTS

DEPENDENCIES = ["sphero_bb8"]

SpheroBB8ScriptSelect = sphero_bb8_ns.class_(
    "SpheroBB8ScriptSelect", select.Select, cg.Component
)

CONF_SPHERO_BB8_ID = "sphero_bb8_id"
CONF_SCRIPTS_FILTER = "scripts"

CONFIG_SCHEMA = select.select_schema(
    SpheroBB8ScriptSelect,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.GenerateID(CONF_SPHERO_BB8_ID): cv.use_id(SpheroBB8),
        # Optional subset of script names to show; defaults to all installed
        # scripts. Lets a device keep per-script buttons for favorites and a
        # dropdown for the rest.
        cv.Optional(CONF_SCRIPTS_FILTER): cv.ensure_list(cv.string),
    }
)


async def to_code(config):
    # Script names are recorded on the hub in definition order by the hub's
    # to_code (see __init__.py); they become the select's codegen-time options.
    names = CORE.data["sphero_bb8"][CONF_SCRIPTS]
    if not names:
        raise cv.Invalid(
            "sphero_bb8 select requires at least one script to be defined in the "
            "sphero_bb8 hub configuration"
        )

    if CONF_SCRIPTS_FILTER in config:
        wanted = config[CONF_SCRIPTS_FILTER]
        unknown = [n for n in wanted if n not in names]
        if unknown:
            raise cv.Invalid(
                f"sphero_bb8 select references unknown script(s): {', '.join(unknown)}. "
                f"Installed scripts: {', '.join(names)}"
            )
        # Preserve the hub's definition order, filtered to the requested set.
        names = [n for n in names if n in set(wanted)]
        if not names:
            raise cv.Invalid("sphero_bb8 select scripts filter matched no scripts")

    parent = await cg.get_variable(config[CONF_SPHERO_BB8_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    await select.register_select(var, config, options=names)
    # Hub notifies every registered select when any script starts, so the
    # dropdown follows launches from buttons and automations too.
    cg.add(parent.register_script_select(var))
