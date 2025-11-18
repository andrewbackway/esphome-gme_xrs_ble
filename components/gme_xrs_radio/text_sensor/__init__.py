import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSTextSensorType,
)

CONF_XRS_ID = "xrs_id"

XRSTextSensorTypeMap = {
    "manufacturer": XRSTextSensorType.XRS_TEXT_MANUFACTURER,
    "model": XRSTextSensorType.XRS_TEXT_MODEL,
    "firmware": XRSTextSensorType.XRS_TEXT_FIRMWARE,
    "serial": XRSTextSensorType.XRS_TEXT_SERIAL,
    "last_message": XRSTextSensorType.XRS_TEXT_LAST_MESSAGE,
    "power_state": XRSTextSensorType.XRS_TEXT_POWER_STATE,
    "ptt_state": XRSTextSensorType.XRS_TEXT_PTT_STATE,
    "channel_label": XRSTextSensorType.XRS_TEXT_CHANNEL_LABEL,
}

CONFIG_SCHEMA = text_sensor.text_sensor_schema(text_sensor.TextSensor).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(text_sensor.TextSensor),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSTextSensorTypeMap, lower=True),
    }
)


async def to_code(config):
    ts = await text_sensor.new_text_sensor(config)
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_text_sensor(config[CONF_TYPE], ts))
