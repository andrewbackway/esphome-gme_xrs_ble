import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import button
from esphome.const import CONF_ID

from .. import XRSRadioComponent, gme_xrs_radio_ns

CONF_GME_XRS_ID = "gme_xrs_id"

XRSRadioButton = gme_xrs_radio_ns.class_(
    "XRSRadioButton", button.Button, cg.Component
)

CONFIG_SCHEMA = button.button_schema(XRSRadioButton).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(XRSRadioButton),
        cv.GenerateID(CONF_GME_XRS_ID): cv.use_id(XRSRadioComponent),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_GME_XRS_ID])
    btn = await button.new_button(config)
    cg.add(btn.set_parent(hub))
