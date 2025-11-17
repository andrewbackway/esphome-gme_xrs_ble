import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import select
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSSelectType,
)

CONF_XRS_ID = "xrs_id"

XRSSelectTypeMap = {
    "zone": XRSSelectType.XRS_SELECT_ZONE,
    "channel": XRSSelectType.XRS_SELECT_CHANNEL,
}

CONFIG_SCHEMA = select.select_schema().extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(select.Select),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSSelectTypeMap, lower=True),
    }
)


async def to_code(config):
    sel = await select.new_select(config)
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_select(config[CONF_TYPE], sel))
