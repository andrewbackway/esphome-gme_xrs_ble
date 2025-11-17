import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import switch
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSSwitchType,
)

CONF_XRS_ID = "xrs_id"

XRSSwitchTypeMap = {
    "location_mode": XRSSwitchType.XRS_SWITCH_LOCATION_MODE,
    "scan": XRSSwitchType.XRS_SWITCH_SCAN,
    "duplex": XRSSwitchType.XRS_SWITCH_DUPLEX,
    "quiet_mode": XRSSwitchType.XRS_SWITCH_QUIET_MODE,
    "quiet_memory": XRSSwitchType.XRS_SWITCH_QUIET_MEMORY,
    "silent_memory": XRSSwitchType.XRS_SWITCH_SILENT_MEMORY,
}

CONFIG_SCHEMA = switch.switch_schema().extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(switch.Switch),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSSwitchTypeMap, lower=True),
    }
)


async def to_code(config):
    sw = await switch.new_switch(config)
    hub = await cg.get_variable(config[CONF_XRS_ID])
    cg.add(hub.register_switch(config[CONF_TYPE], sw))
