import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
)

from .. import (
    XRSRadioComponent,
    XRSNumberType,
    xrs_radio_ns,
)

CONF_GME_XRS_ID = "gme_xrs_id"

XRSRadioNumber = xrs_radio_ns.class_(
    "XRSRadioNumber", number.Number, cg.Component
)

XRSNumberTypeMap = {
    "volume": XRSNumberType.XRS_NUMBER_VOLUME,
}

CONFIG_SCHEMA = number.number_schema(XRSRadioNumber).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(XRSRadioNumber),
        cv.GenerateID(CONF_GME_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSNumberTypeMap, lower=True),
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=31.0): cv.float_,
        cv.Optional(CONF_STEP, default=1.0): cv.positive_float,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GME_XRS_ID])
    type_enum = XRSNumberTypeMap[config[CONF_TYPE]]

    num = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    cg.add(num.set_parent(parent))
    cg.add(num.set_type(type_enum))
    cg.add(parent.register_number(type_enum, num))
