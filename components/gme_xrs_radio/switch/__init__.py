import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import switch
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    XRSRadioComponent,
    XRSSwitchType,
    xrs_radio_ns,
)

CONF_GME_XRS_ID = "gme_xrs_id"

XRSRadioSwitch = xrs_radio_ns.class_(
    "XRSRadioSwitch", switch.Switch, cg.Component
)

XRSSwitchTypeMap = {
    "location_mode": XRSSwitchType.XRS_SWITCH_LOCATION_MODE,
    "scan": XRSSwitchType.XRS_SWITCH_SCAN,
    "duplex": XRSSwitchType.XRS_SWITCH_DUPLEX,
    "quiet_mode": XRSSwitchType.XRS_SWITCH_QUIET_MODE,
    "quiet_memory": XRSSwitchType.XRS_SWITCH_QUIET_MEMORY,
    "silent_memory": XRSSwitchType.XRS_SWITCH_SILENT_MEMORY,
}

CONFIG_SCHEMA = switch.switch_schema(XRSRadioSwitch).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(XRSRadioSwitch),
        cv.GenerateID(CONF_GME_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRSSwitchTypeMap, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GME_XRS_ID])
    type_enum = XRSSwitchTypeMap[config[CONF_TYPE]]

    sw = await switch.new_switch(config)
    cg.add(sw.set_parent(parent))
    cg.add(sw.set_type(type_enum))
    cg.add(parent.register_switch(type_enum, sw))
