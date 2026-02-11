import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

AUTO_LOAD = ["light", "button", "text_sensor", "sensor", "binary_sensor"]

sphero_bb8_ns = cg.esphome_ns.namespace("sphero_bb8")
SpheroBB8 = sphero_bb8_ns.class_("SpheroBB8", cg.Component, ble_client.BLEClientNode)

CONF_SPHERO_BB8_ID = "sphero_bb8_id"
CONF_AUTO_CONNECT = "auto_connect"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SpheroBB8),
            cv.Optional(CONF_AUTO_CONNECT, default=False): cv.boolean,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_auto_connect(config[CONF_AUTO_CONNECT]))
