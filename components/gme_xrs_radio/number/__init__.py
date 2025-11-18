import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import number
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSNumberType,
)

CONF_XRS_ID = "xrs_id"

XRSNumberTypeMap = {
    "volume": XRSNumberType.XRS_NUMBER_VOLUME,
}

CONFIG_SCHEMA = number.number_schema(number.Number).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(number.Number),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSNumberTypeMap, lower=True),
    }
)


async def to_code(config):
    num = await number.new_number(config)
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_number(config[CONF_TYPE], num))
