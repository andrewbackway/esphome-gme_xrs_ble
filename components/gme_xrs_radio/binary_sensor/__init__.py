import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSBinarySensorType,
)

CONF_XRS_ID = "gme_xrs_id"

XRSBinarySensorTypeMap = {
    "connected": XRSBinarySensorType.XRS_BIN_CONNECTED,
    "ptt_active": XRSBinarySensorType.XRS_BIN_PTT_ACTIVE,
    "ptt_data": XRSBinarySensorType.XRS_BIN_PTT_DATA,
    "power_low": XRSBinarySensorType.XRS_BIN_POWER_LOW,
}

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    binary_sensor.BinarySensor
).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(binary_sensor.BinarySensor),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSBinarySensorTypeMap, lower=True),
    }
)


async def to_code(config):
    bs = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_binary_sensor(config[CONF_TYPE], bs))
