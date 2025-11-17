import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import ble_client, text_sensor
from esphome.const import CONF_ID

CONF_STATUS_TEXT = "status_text"

gme_xrs_radio_ns = cg.esphome_ns.namespace("gme_xrs_radio")

GmeXrsRadioComponent = gme_xrs_radio_ns.class_(
    "GmeXrsRadioComponent",
    cg.Component,
    ble_client.BLEClientNode,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GmeXrsRadioComponent),

        # Link to the existing ble_client:
        cv.Required("ble_client_id"): cv.use_id(ble_client.BLEClient),

        # Optional diagnostic text sensor – last raw line:
        cv.Optional(CONF_STATUS_TEXT): text_sensor.text_sensor_schema(
            icon="mdi:radio-tower",
            entity_category="diagnostic",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Wire into the BLE client
    ble_parent = await cg.get_variable(config["ble_client_id"])
    cg.add(ble_parent.register_ble_node(var))

    # Optional status text sensor
    if CONF_STATUS_TEXT in config:
        ts = await text_sensor.new_text_sensor(config[CONF_STATUS_TEXT])
        cg.add(var.set_status_text_sensor(ts))
