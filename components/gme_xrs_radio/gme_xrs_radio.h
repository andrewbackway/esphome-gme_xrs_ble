#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32

#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"

#include <string>
#include <vector>

namespace esphome {
namespace gme_xrs_radio {

namespace espbt = esphome::esp32_ble_tracker;
namespace esp32_ble = esphome::esp32_ble;
namespace esp32_ble_client = esphome::esp32_ble_client;

class GmeXrsRadioComponent : public Component, public ble_client::BLEClientNode {
 public:
  void set_status_text_sensor(text_sensor::TextSensor *status) { status_text_sensor_ = status; }

  // Component interface
  void setup() override;
  void loop() override;
  void dump_config() override;

  // BLE client callbacks
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  // Public API for automations / other components:
  // Sends a raw ASCII command; \r\n is added if missing.
  void send_raw_command(const std::string &cmd);

 protected:
  // GATT helpers
  void handle_search_complete_();
  void handle_notify_(uint16_t handle, const uint8_t *data, uint16_t length);
  void ensure_paired_();
  bool is_client_ready_();

  // RX line handling
  void process_rx_buffer_();
  void handle_line_(const std::string &line);
  void publish_status_(const std::string &line);

  // BLE handles
  esp32_ble_client::BLECharacteristic *tx_char_{nullptr};
  std::vector<uint16_t> notify_handles_;

  // Simple line buffer for ASCII protocol
  std::string rx_buffer_;

  // Optional diagnostic text sensor
  text_sensor::TextSensor *status_text_sensor_{nullptr};
};

}  // namespace gme_xrs_radio
}  // namespace esphome

#endif  // USE_ESP32
