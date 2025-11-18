#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"
#include "xrs_at_parser.h"  // ATParser in namespace esphome::gme_xrs_radio

namespace esphome {
namespace gme_xrs_radio {

class XRSRadioSelect;
class XRSRadioNumber;
class XRSRadioSwitch;

namespace espbt = esphome::esp32_ble_tracker;
namespace esp32_ble = esphome::esp32_ble;
namespace esp32_ble_client = esphome::esp32_ble_client;

// -----------------------------------------------------------------------------
// Enums describing the per-entity "type" from YAML
// -----------------------------------------------------------------------------

enum XRSNumericSensorType {
  XRS_SENSOR_CHANNEL,
  XRS_SENSOR_ZONE,
  XRS_SENSOR_VOLUME,
  XRS_SENSOR_PTT_TIMER,
};

enum XRSBinarySensorType {
  XRS_BIN_CONNECTED,
  XRS_BIN_PTT_ACTIVE,
  XRS_BIN_PTT_DATA,
  XRS_BIN_POWER_LOW,
  XRS_BIN_SCANNING,
  XRS_BIN_DUPLEX_ENABLED,
  XRS_BIN_SILENT_MEMORY,
  XRS_BIN_QUIET_MEMORY,
  XRS_BIN_QUIET_MODE,
};

enum XRSTextSensorType {
  XRS_TEXT_MANUFACTURER,
  XRS_TEXT_MODEL,
  XRS_TEXT_FIRMWARE,
  XRS_TEXT_SERIAL,
  XRS_TEXT_LAST_MESSAGE,
  XRS_TEXT_POWER_STATE,
  XRS_TEXT_PTT_STATE,
  XRS_TEXT_CHANNEL_LABEL,
};

enum XRSNumberType {
  XRS_NUMBER_VOLUME,
};

enum XRSSwitchType {
  XRS_SWITCH_LOCATION_MODE,
  XRS_SWITCH_SCAN,
  XRS_SWITCH_DUPLEX,
  XRS_SWITCH_QUIET_MODE,
  XRS_SWITCH_QUIET_MEMORY,
  XRS_SWITCH_SILENT_MEMORY,
};

enum XRSSelectType : uint8_t {
  XRS_SELECT_ZONE,
  XRS_SELECT_CHANNEL,
};

// Simple channel metadata used for labels/options
struct ChannelInfo {
  uint8_t zone;
  uint8_t channel;
  std::string name;  // display label
};

// -----------------------------------------------------------------------------
// Main hub component - BLE transport + AT parser + radio state
// -----------------------------------------------------------------------------

class XRSRadioComponent : public Component,
                          public ble_client::BLEClientNode,
                          public gme_xrs_radio::ATParserListener {
 public:
  XRSRadioComponent();

  // Component interface
  void setup() override;
  void loop() override;
  void dump_config() override;

  // BLE client callbacks
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t* param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event,
                         esp_ble_gap_cb_param_t* param) override;

  // Registration from Python platforms
  void register_numeric_sensor(XRSNumericSensorType type,
                               sensor::Sensor* sensor);
  void register_binary_sensor(XRSBinarySensorType type,
                              binary_sensor::BinarySensor* sensor);
  void register_text_sensor(XRSTextSensorType type,
                            text_sensor::TextSensor* sensor);

  void register_number(XRSNumberType type, XRSRadioNumber* num);

  void register_switch(XRSSwitchType type, XRSRadioSwitch* sw);
  void register_select(XRSSelectType type, XRSRadioSelect* sel);

  // Optional debug/text sensor for last raw line/result.
  void set_status_text_sensor(text_sensor::TextSensor* status) {
    status_text_sensor_ = status;
  }

  // Location configuration (dynamic lat/long from other sensors).
  void set_latitude_sensor(sensor::Sensor* sensor) {
    latitude_sensor_ = sensor;
  }
  void set_longitude_sensor(sensor::Sensor* sensor) {
    longitude_sensor_ = sensor;
  }
  void set_location_interval(uint32_t interval_ms) {
    location_interval_ms_ = interval_ms;
  }

  // Public control API used by wrapper entities
  void set_volume(uint8_t volume);
  void set_location_mode(bool enabled);
  void set_scan_enabled(bool enabled);
  void set_duplex_enabled(bool enabled);
  void set_quiet_mode(bool enabled);
  void set_quiet_memory(bool enabled);
  void set_silent_memory(bool enabled);

  void set_zone(uint8_t zone);
  void set_channel(uint8_t zone, uint8_t channel);
  uint8_t get_current_zone() const { return this->current_zone_; }
  uint8_t get_current_channel() const { return this->current_channel_; }

  // For select entities: list of "Z1", "Z2", ... and "Z1 / Ch 01" style labels.
  std::vector<std::string> get_zone_options() const;
  std::vector<std::string> get_channel_options() const;

  // ATParserListener implementation
  void on_result_code(gme_xrs_radio::ATResultCode code) override;
  void on_plus_line(const std::string& name,
                    const std::string& payload) override;
  void on_info_line(const std::string& line) override;
  void on_echo(const std::string& line) override;
  void on_unknown_line(const std::string& line) override;

  // Low-level: send a raw ASCII AT command (adds CR/LF if missing).
  void send_raw_command(const std::string& cmd);

 protected:
  // BLE helpers
  void handle_search_complete_();
  void handle_notify_(uint16_t handle, const uint8_t* data, uint16_t length);
  void ensure_paired_();
  bool is_client_ready_();

  // State publishing helpers
  void publish_status_(const std::string& line);
  void publish_all_state_();
  void publish_connection_state_(bool connected);

  // Handlers for particular AT notifications / responses
  void handle_plus_wgchs_(const std::string& payload);   // channel set
  void handle_plus_whzs_(const std::string& payload);    // zone set
  void handle_plus_wgptt_(const std::string& payload);   // PTT state
  void handle_plus_wgpow_(const std::string& payload);   // power state
  void handle_plus_wgscan_(const std::string& payload);  // scan state
  void handle_plus_wgdup_(const std::string& payload);   // duplex
  void handle_plus_wgcsm_(const std::string& payload);   // silent memory
  void handle_plus_wgsqm_(const std::string& payload);   // quiet memory
  void handle_plus_wgssq_(const std::string& payload);   // quiet mode
  void handle_plus_wgchsq_(
      const std::string& payload);  // channel table (if present)
  void handle_plus_gmi_(const std::string& payload);
  void handle_plus_gmm_(const std::string& payload);
  void handle_plus_gmr_(const std::string& payload);
  void handle_plus_gsn_(const std::string& payload);

  // Location upload
  void send_location_update_();

  // Resolve a channel label from the table (or a simple fallback).
  std::string get_channel_label_(uint8_t zone, uint8_t channel) const;

  // BLE handles
  esp32_ble_client::BLECharacteristic* tx_char_{nullptr};
  std::vector<uint16_t> notify_handles_;

  // AT parser
  gme_xrs_radio::ATParser at_parser_;

  // Core radio state
  bool connected_{false};

  uint8_t current_zone_{0};
  uint8_t current_channel_{0};
  uint8_t current_volume_{0};

  // PTT state: 0 = idle, 1 = voice, 2 = voice+data
  uint8_t ptt_state_{0};
  float ptt_timer_seconds_{0.0f};

  // Power state 0..5 as per WGPOW; text map exposed via text sensor.
  uint8_t power_state_{0};

  bool scanning_{false};
  bool duplex_enabled_{false};
  bool silent_memory_{false};
  bool quiet_memory_{false};
  bool quiet_mode_{false};

  // Device identity
  std::string manufacturer_;
  std::string model_;
  std::string firmware_;
  std::string serial_;

  // Location upload configuration/state.
  sensor::Sensor* latitude_sensor_{nullptr};
  sensor::Sensor* longitude_sensor_{nullptr};
  bool location_mode_{false};
  uint32_t location_interval_ms_{60000};
  uint32_t last_location_sent_{0};

  // Channel table from radio.
  std::vector<ChannelInfo> channel_table_;

  // Registered entities
  sensor::Sensor* sensor_channel_{nullptr};
  sensor::Sensor* sensor_zone_{nullptr};
  sensor::Sensor* sensor_volume_{nullptr};
  sensor::Sensor* sensor_ptt_timer_{nullptr};

  binary_sensor::BinarySensor* bin_connected_{nullptr};
  binary_sensor::BinarySensor* bin_ptt_active_{nullptr};
  binary_sensor::BinarySensor* bin_ptt_data_{nullptr};
  binary_sensor::BinarySensor* bin_power_low_{nullptr};
  binary_sensor::BinarySensor* bin_scanning_{nullptr};
  binary_sensor::BinarySensor* bin_duplex_enabled_{nullptr};
  binary_sensor::BinarySensor* bin_silent_memory_{nullptr};
  binary_sensor::BinarySensor* bin_quiet_memory_{nullptr};
  binary_sensor::BinarySensor* bin_quiet_mode_{nullptr};

  text_sensor::TextSensor* text_manufacturer_{nullptr};
  text_sensor::TextSensor* text_model_{nullptr};
  text_sensor::TextSensor* text_firmware_{nullptr};
  text_sensor::TextSensor* text_serial_{nullptr};
  text_sensor::TextSensor* text_last_message_{nullptr};
  text_sensor::TextSensor* text_power_state_{nullptr};
  text_sensor::TextSensor* text_ptt_state_{nullptr};
  text_sensor::TextSensor* text_channel_label_{nullptr};
  text_sensor::TextSensor* status_text_sensor_{nullptr};

  number::Number* num_volume_{nullptr};

  XRSRadioSwitch *sw_location_mode_{nullptr};
  XRSRadioSwitch *sw_scan_{nullptr};
  XRSRadioSwitch *sw_duplex_{nullptr};
  XRSRadioSwitch *sw_quiet_mode_{nullptr};
  XRSRadioSwitch *sw_quiet_memory_{nullptr};
  XRSRadioSwitch *sw_silent_memory_{nullptr};

  XRSRadioSelect* sel_zone_{nullptr};
  XRSRadioSelect* sel_channel_{nullptr};
};

}  // namespace gme_xrs_radio
}  // namespace esphome

#endif  // USE_ESP32
