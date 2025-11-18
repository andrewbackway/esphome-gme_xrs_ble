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
)

CONF_XRS_ID = "gme_xrs_id"

XRSNumberTypeMap = {
    "volume": XRSNumberType.XRS_NUMBER_VOLUME,
}

# Base schema for a standard Number entity
CONFIG_SCHEMA = number.number_schema(number.Number).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(number.Number),

        # Link back to the main hub
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),

        # Logical type (only "volume" for now)
        cv.Required(CONF_TYPE): cv.enum(XRSNumberTypeMap, lower=True),

        # Range config for this number
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=31.0): cv.float_,
        cv.Optional(CONF_STEP, default=1.0): cv.positive_float,
    }
)


async def to_code(config):
    # Create the Number entity with the configured range
    num = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    # Attach it to the XRS hub
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_number(config[CONF_TYPE], num))
