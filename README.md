# ESPHome GME XRS Radio Component

An ESPHome custom component for integrating GME XRS series radios via Bluetooth Low Energy (BLE). This component enables monitoring and control of your GME XRS radio through ESPHome, making it accessible through Home Assistant and other platforms.

## Features

- **Radio Control**: Change channels, zones, volume, and various radio settings
- **Status Monitoring**: Monitor connection status, PTT activity, power state, and more
- **Location Sharing**: Automatically send GPS coordinates to the radio
- **Remote Data**: Receive and display remote user information (location, messages, etc.)
- **Native ESPHome Integration**: Works seamlessly with Home Assistant and other ESPHome-compatible platforms

## Supported Radios

This component supports GME XRS series radios that communicate using the XRS AT Command Protocol over BLE. For detailed protocol documentation, see [protocol.md](protocol.md).

## Requirements

- **Hardware**: ESP32 board with BLE support (tested on Lolin S3 Pro)
- **Software**: ESPHome 2023.x or later
- **Radio**: GME XRS series radio with Bluetooth capability

## Installation

### Method 1: Direct GitHub Reference (Recommended)

Add the following to your ESPHome YAML configuration:

```yaml
external_components:
  - source: github://andrewbackway/esphome-gme_xrs_radio
    components: [ gme_xrs_radio ]
```

### Method 2: Local Components

1. Clone this repository
2. Copy the `components/gme_xrs_radio` folder to your ESPHome configuration directory
3. Reference it in your YAML:

```yaml
external_components:
  - source: components
    components: [ gme_xrs_radio ]
```

## Basic Configuration

### Minimal Setup

```yaml
esphome:
  name: gme-xrs-radio
  friendly_name: GME XRS Radio

esp32:
  board: lolin_s3_pro
  framework:
    type: esp-idf

# Enable BLE tracking
esp32_ble_tracker:

# Configure BLE client
ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"  # Your radio's MAC address
    id: gme_xrs_ble_client
    auto_connect: true

# Add the radio component
gme_xrs_radio:
  - id: my_xrs_radio
    ble_client_id: gme_xrs_ble_client
```

### Full Configuration with Location Sharing

```yaml
gme_xrs_radio:
  - id: my_xrs_radio
    ble_client_id: gme_xrs_ble_client
    location_interval: 60s  # How often to send location (default: 60s)
    latitude: latitude_sensor  # Reference to a sensor providing GPS latitude
    longitude: longitude_sensor  # Reference to a sensor providing GPS longitude
    message: status_message  # Reference to a text sensor for status messages
```

## Available Components

### Binary Sensors

Monitor boolean states from the radio:

```yaml
binary_sensor:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    connected:
      name: "Radio Connected"
    ptt_active:
      name: "PTT Active"
    ptt_data:
      name: "PTT Data"
    power_low:
      name: "Power Low"
    scanning:
      name: "Radio Scanning"
```

### Sensors

Numeric values from the radio:

```yaml
sensor:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    ptt_timer:
      name: "PTT Timer"
    remote_seq:
      name: "Remote Sequence"
    remote_latitude:
      name: "Remote Latitude"
    remote_longitude:
      name: "Remote Longitude"
```

### Text Sensors

String values from the radio:

```yaml
text_sensor:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    manufacturer:
      name: "Radio Manufacturer"
    model:
      name: "Radio Model"
    firmware:
      name: "Radio Firmware"
    serial:
      name: "Radio Serial Number"
    last_message:
      name: "Last Message"
    power_state:
      name: "Power State"
    ptt_state:
      name: "PTT State"
    channel_label:
      name: "Current Channel"
    remote_uid:
      name: "Remote User ID"
    remote_message:
      name: "Remote Message"
    remote_time:
      name: "Remote Time"
```

### Switches

Toggle radio features:

```yaml
switch:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    location_mode:
      name: "Location Sharing"
    scan:
      name: "Scan Mode"
    duplex:
      name: "Duplex Mode"
    quiet_mode:
      name: "Quiet Mode"
    quiet_memory:
      name: "Quiet Memory"
    silent_memory:
      name: "Silent Memory"
```

### Numbers

Adjust numeric settings:

```yaml
number:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    volume:
      name: "Radio Volume"
```

### Selects

Choose from predefined options:

```yaml
select:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    zone:
      name: "Zone"
    channel:
      name: "Channel"
```

### Buttons

Trigger actions (if implemented):

```yaml
button:
  - platform: gme_xrs_radio
    gme_xrs_radio_id: my_xrs_radio
    # Button types would be defined here
```

## Finding Your Radio's MAC Address

1. Enable Bluetooth on your device
2. Pair with your GME XRS radio
3. Use a Bluetooth scanner app to find the MAC address
4. On Android: Settings → Bluetooth → Paired devices → Tap the radio → Device details
5. On Windows: Settings → Bluetooth & devices → View devices → Right-click radio → Properties

## Complete Example

See [example.yaml](example.yaml) for a complete working configuration.

## Troubleshooting

### Radio Won't Connect

- Ensure the MAC address is correct
- Make sure the radio is powered on and within range
- Verify the radio is not already connected to another device
- Check that Bluetooth is enabled on the ESP32

### Location Not Updating

- Verify `latitude` and `longitude` sensors are properly configured
- Check that the sensors are providing valid coordinates
- Ensure `location_interval` is set appropriately
- Review logs for any error messages

### Debug Logging

Enable debug logging to troubleshoot issues:

```yaml
logger:
  level: DEBUG
  logs:
    component: DEBUG
    gme_xrs_radio: DEBUG
```

## Protocol Documentation

For detailed information about the XRS AT Command Protocol used by this component, see [protocol.md](protocol.md).

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is open source. Please check the repository for specific license information.

## Acknowledgments

- GME for their XRS radio series (feel free to send me gear, you guys make quality stuff!)
- ESPHome community for the excellent framework
- Protocol analysis based on observed behavior of the Android app and published datasheet.

## Disclaimer

This component is based on protocol analysis and is not officially supported by GME. Use at your own risk.
