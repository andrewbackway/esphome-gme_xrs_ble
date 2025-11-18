import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_ID, CONF_TYPE
from esphome.components import select as select_base

from .. import XRSRadioComponent, XRSSelectType, xrs_radio_ns

CONF_XRS_ID = "xrs_id"

# C++ class: class XRSRadioSelect : public select::Select, public Component
XRSRadioSelect = xrs_radio_ns.class_("XRSRadioSelect", select_base.Select, cg.Component)

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

    # In 2025.10+ new_select() wants 'options', but we pass an empty list
    # because options are provided dynamically from C++.
    var = await select_base.new_select(config, options=[])

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(type_enum))
    cg.add(parent.register_select(type_enum, var))
