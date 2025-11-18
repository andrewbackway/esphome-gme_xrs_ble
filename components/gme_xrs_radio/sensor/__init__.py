import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSNumericSensorType,
)

CONF_XRS_ID = "gme_xrs_id"

XRSNumericSensorTypeMap = {
    "volume": XRSNumericSensorType.XRS_SENSOR_VOLUME,
    "ptt_timer": XRSNumericSensorType.XRS_SENSOR_PTT_TIMER,
    "remote_seq": XRSNumericSensorType.XRS_SENSOR_REMOTE_SEQ,
    "remote_latitude": XRSNumericSensorType.XRS_SENSOR_REMOTE_LATITUDE,
    "remote_longitude": XRSNumericSensorType.XRS_SENSOR_REMOTE_LONGITUDE,
}

CONFIG_SCHEMA = sensor.sensor_schema(sensor.Sensor).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(sensor.Sensor),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSNumericSensorTypeMap, lower=True),
    }
)


async def to_code(config):
    sens = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_numeric_sensor(config[CONF_TYPE], sens))
