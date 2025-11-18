import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_ID, CONF_TYPE
from esphome.components import select as select_base

from .. import XRSRadioComponent, XRSSelectType, xrs_radio_ns

CONF_XRS_ID = "xrs_id"

XRSRadioSelect = xrs_radio_ns.class_(
    "XRSRadioSelect", select_base.Select, cg.Component
)

XRS_RADIO_SELECT_TYPES = {
    "zone": XRSSelectType.XRS_SELECT_ZONE,
    "channel": XRSSelectType.XRS_SELECT_CHANNEL,
}

CONFIG_SCHEMA = select_base.select_schema(XRSRadioSelect).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(XRSRadioSelect),
        cv.GenerateID(CONF_XRS_ID): cv.use_id(XRSRadioComponent),
        cv.Required(CONF_TYPE): cv.enum(XRS_RADIO_SELECT_TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_XRS_ID])
    type_enum = XRS_RADIO_SELECT_TYPES[config[CONF_TYPE]]

    # new_select in recent ESPHome wants options kwarg; we give [] and let C++
    # supply options dynamically via get_traits()/refresh_from_parent().
    var = await select_base.new_select(config, options=[])

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(type_enum))
    cg.add(parent.register_select(type_enum, var))
