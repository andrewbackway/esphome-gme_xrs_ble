import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import ble_client, sensor, text_sensor
from esphome.const import CONF_ID

from esphome.components.ble_client import CONF_BLE_CLIENT_ID

CONF_LATITUDE = "latitude"
CONF_LONGITUDE = "longitude"
CONF_LOCATION_INTERVAL = "location_interval"

gme_xrs_radio_ns = cg.esphome_ns.namespace("gme_xrs_radio")

XRSRadioComponent = gme_xrs_radio_ns.class_(
    "XRSRadioComponent",
    cg.Component,
    ble_client.BLEClientNode,
)

# Enums – must match the C++ names
XRSNumericSensorType = gme_xrs_radio_ns.enum("XRSNumericSensorType")
XRSBinarySensorType = gme_xrs_radio_ns.enum("XRSBinarySensorType")
XRSTextSensorType = gme_xrs_radio_ns.enum("XRSTextSensorType")
XRSNumberType = gme_xrs_radio_ns.enum("XRSNumberType")
XRSSwitchType = gme_xrs_radio_ns.enum("XRSSwitchType")
XRSSelectType = gme_xrs_radio_ns.enum("XRSSelectType")

AUTO_LOAD = ["ble_client"]

CONFIG_SCHEMA = (
    ble_client.BLE_CLIENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(XRSRadioComponent),
            cv.Optional(CONF_LATITUDE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_LONGITUDE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_LOCATION_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Wire into the BLE client
    ble_parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(ble_parent.register_ble_node(var))

    # Optional latitude / longitude sensors for dynamic location
    if CONF_LATITUDE in config:
        lat = await cg.get_variable(config[CONF_LATITUDE])
        cg.add(var.set_latitude_sensor(lat))

    if CONF_LONGITUDE in config:
        lon = await cg.get_variable(config[CONF_LONGITUDE])
        cg.add(var.set_longitude_sensor(lon))

    # Location update interval (ms)
    cg.add(var.set_location_interval(config[CONF_LOCATION_INTERVAL]))
