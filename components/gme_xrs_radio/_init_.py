import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import ble_client, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["text_sensor"]

CONF_STATUS_TEXT = "status_text"

gme_xrs_radio_ns = cg.esphome_ns.namespace("gme_xrs_radio")
GmeXrsRadioComponent = gme_xrs_radio_ns.class_(
    "GmeXrsRadioComponent",
    cg.Component,
    ble_client.BLEClientNode,
)

CONFIG_SCHEMA = ble_client.BLE_CLIENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GmeXrsRadioComponent),
        cv.Optional(CONF_STATUS_TEXT): text_sensor.text_sensor_schema(
            icon="mdi:radio-tower",
            entity_category="diagnostic",
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_STATUS_TEXT in config:
        ts = await text_sensor.new_text_sensor(config[CONF_STATUS_TEXT])
        cg.add(var.set_status_text_sensor(ts))
