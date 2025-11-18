import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import number
from esphome.const import (
    CONF_TYPE,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
)

from .. import (
    gme_xrs_radio_ns,
    GmeXrsRadioComponent,
    CONF_XRS_ID,
    XRSNumberTypeMap,
)

DEPENDENCIES = ["gme_xrs_radio"]

# We use the built-in Number base class
XrsNumber = number.Number

CONFIG_SCHEMA = number.number_schema(XrsNumber).extend(
    {
        # Link back to the main GME XRS component
        cv.GenerateID(CONF_XRS_ID): cv.use_id(GmeXrsRadioComponent),

        # Which logical number this is (volume, etc.)
        cv.Required(CONF_TYPE): cv.enum(XRSNumberTypeMap, lower=True),

        # User-configurable range
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

    # Attach it to the main XRS component
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_number(config[CONF_TYPE], num))
