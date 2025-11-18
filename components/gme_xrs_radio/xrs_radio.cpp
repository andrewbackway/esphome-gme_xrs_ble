#include "xrs_radio.h"

#include <cmath>

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "number/xrs_number.h"
#include "select/xrs_select.h"
#include "switch/xrs_switch.h"

namespace esphome {
namespace gme_xrs_radio {

static const char* const TAG = "gme_xrs_radio";

// GME XRS primary service UUID
static const char* const GME_XRS_SERVICE_STR =
    "49535343-fe7d-4ae5-8fa9-9fafd205e455";

// Command TX characteristic (write)
static const char* const GME_XRS_CHAR_TX_STR =
    "49535343-8841-43f4-a8d4-ecbe34729bb3";

// Notify/stream characteristics
static const char* const GME_XRS_CHAR_NOTIFY_MAIN_STR =
    "49535343-1e4d-4bd9-ba61-23c647249616";
static const char* const GME_XRS_CHAR_NOTIFY_AUX1_STR =
    "49535343-aca3-481c-91ec-d85e28a60318";
static const char* const GME_XRS_CHAR_NOTIFY_AUX2_STR =
    "49535343-026e-3a9b-954c-97daef17e26e";

static bool g_security_configured = false;

// Configure BLE security to match the GME XRS expectations.
static void configure_gme_xrs_security() {
  // Auth requirement: LE Secure Connections + MITM + Bonding
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;

  // 16-byte key (max)
  uint8_t key_size = 16;

  // Exchange encryption and identity keys both ways
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t resp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

  // No out-of-band
  uint8_t oob_enable = ESP_BLE_OOB_DISABLE;

  esp_err_t err;

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req,
                                       sizeof(auth_req));
  ESP_LOGI(TAG, "XRS: set AUTHEN_REQ_MODE=0x%02X -> %s", auth_req,
           esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size,
                                       sizeof(key_size));
  ESP_LOGI(TAG, "XRS: set MAX_KEY_SIZE=%u -> %s", key_size,
           esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key,
                                       sizeof(init_key));
  ESP_LOGI(TAG, "XRS: set INIT_KEY mask=0x%02X -> %s", init_key,
           esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &resp_key,
                                       sizeof(resp_key));
  ESP_LOGI(TAG, "XRS: set RSP_KEY mask=0x%02X -> %s", resp_key,
           esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_enable,
                                       sizeof(oob_enable));
  ESP_LOGI(TAG, "XRS: set OOB_SUPPORT=%u -> %s", oob_enable,
           esp_err_to_name(err));
}

XRSRadioComponent::XRSRadioComponent() : at_parser_(this) {}

void XRSRadioComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XRS radio over BLE...");
  this->node_state = espbt::ClientState::INIT;
}

void XRSRadioComponent::loop() {
  // Periodic location upload if enabled
  uint32_t now = millis();
  if (this->location_mode_ && this->is_client_ready_()) {
    if (now - this->last_location_sent_ >= this->location_interval_ms_) {
      this->last_location_sent_ = now;
      this->send_location_update_();
    }
  }
}

void XRSRadioComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "XRS Radio (BLE)");
  ESP_LOGCONFIG(TAG, "  Service UUID: %s", GME_XRS_SERVICE_STR);
  if (this->parent() != nullptr) {
    ESP_LOGCONFIG(TAG, "  Bound to BLE client index: %u",
                  this->parent()->get_connection_index());
  }
}

// -----------------------------------------------------------------------------
// Registration from Python platforms
// -----------------------------------------------------------------------------

void XRSRadioComponent::register_numeric_sensor(XRSNumericSensorType type,
                                                sensor::Sensor* sensor) {
  switch (type) {
    case XRS_SENSOR_PTT_TIMER:
      this->sensor_ptt_timer_ = sensor;
      break;
    case XRS_SENSOR_REMOTE_SEQ:
      this->sensor_remote_seq_ = sensor;
      break;
    case XRS_SENSOR_REMOTE_LATITUDE:
      this->sensor_remote_latitude_ = sensor;
      break;
    case XRS_SENSOR_REMOTE_LONGITUDE:
      this->sensor_remote_longitude_ = sensor;
      break;
  }
}

void XRSRadioComponent::register_binary_sensor(
    XRSBinarySensorType type, binary_sensor::BinarySensor* sensor) {
  switch (type) {
    case XRS_BIN_CONNECTED:
      this->bin_connected_ = sensor;
      break;
    case XRS_BIN_PTT_ACTIVE:
      this->bin_ptt_active_ = sensor;
      break;
    case XRS_BIN_PTT_DATA:
      this->bin_ptt_data_ = sensor;
      break;
    case XRS_BIN_POWER_LOW:
      this->bin_power_low_ = sensor;
      break;
  }
}

void XRSRadioComponent::register_text_sensor(XRSTextSensorType type,
                                             text_sensor::TextSensor* sensor) {
  switch (type) {
    case XRS_TEXT_MANUFACTURER:
      this->text_manufacturer_ = sensor;
      break;
    case XRS_TEXT_MODEL:
      this->text_model_ = sensor;
      break;
    case XRS_TEXT_FIRMWARE:
      this->text_firmware_ = sensor;
      break;
    case XRS_TEXT_SERIAL:
      this->text_serial_ = sensor;
      break;
    case XRS_TEXT_LAST_MESSAGE:
      this->text_last_message_ = sensor;
      break;
    case XRS_TEXT_POWER_STATE:
      this->text_power_state_ = sensor;
      break;
    case XRS_TEXT_PTT_STATE:
      this->text_ptt_state_ = sensor;
      break;
    case XRS_TEXT_CHANNEL_LABEL:
      this->text_channel_label_ = sensor;
      break;
    case XRS_TEXT_REMOTE_UID:
      this->text_remote_uid_ = sensor;
      break;
    case XRS_TEXT_REMOTE_MESSAGE:
      this->text_remote_message_ = sensor;
      break;
    case XRS_TEXT_REMOTE_TIME:
      this->text_remote_time_ = sensor;
      break;
  }
}

void XRSRadioComponent::register_number(XRSNumberType type,
                                        XRSRadioNumber* num) {
  switch (type) {
    case XRS_NUMBER_VOLUME:
      this->number_volume_ = num;
      break;
    default:
      break;
  }
}

void XRSRadioComponent::register_switch(XRSSwitchType type,
                                        XRSRadioSwitch* sw) {
  switch (type) {
    case XRS_SWITCH_LOCATION_MODE:
      sw_location_mode_ = sw;
      break;
    case XRS_SWITCH_SCAN:
      sw_scan_ = sw;
      break;
    case XRS_SWITCH_DUPLEX:
      sw_duplex_ = sw;
      break;
    case XRS_SWITCH_QUIET_MODE:
      sw_quiet_mode_ = sw;
      break;
    case XRS_SWITCH_QUIET_MEMORY:
      sw_quiet_memory_ = sw;
      break;
    case XRS_SWITCH_SILENT_MEMORY:
      sw_silent_memory_ = sw;
      break;
    default:
      break;
  }
}

void XRSRadioComponent::register_select(XRSSelectType type,
                                        XRSRadioSelect* sel) {
  switch (type) {
    case XRS_SELECT_ZONE:
      this->sel_zone_ = sel;
      break;
    case XRS_SELECT_CHANNEL:
      this->sel_channel_ = sel;
      break;
    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// BLE client callbacks
// -----------------------------------------------------------------------------

void XRSRadioComponent::gattc_event_handler(esp_gattc_cb_event_t event,
                                            esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t* param) {
  (void)gattc_if;

  if (this->parent() == nullptr) {
    return;
  }

  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      ESP_LOGD(TAG, "GATT OPEN_EVT (conn_id=%d, status=0x%02X)",
               param->open.conn_id, param->open.status);

      if (!g_security_configured) {
        ESP_LOGI(TAG, "Configuring BLE security for XRS (on OPEN_EVT)...");
        configure_gme_xrs_security();
        g_security_configured = true;
      }

      if (param->open.conn_id != this->parent()->get_conn_id()) break;

      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "GATT connection opened (conn_id=%d)",
                 param->open.conn_id);
        this->ensure_paired_();
      } else {
        ESP_LOGW(TAG, "Failed to open GATT connection, status=0x%02X",
                 param->open.status);
      }
      break;
    }

    case ESP_GATTC_ENC_CMPL_CB_EVT: {
      ESP_LOGI(TAG, "GATT encryption/security completed for XRS link");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->parent()->get_conn_id()) break;

      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service discovery failed, status=0x%02X",
                 param->search_cmpl.status);
        break;
      }

      ESP_LOGI(TAG, "Service discovery complete, configuring XRS service...");
      this->handle_search_complete_();
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id()) break;

      this->handle_notify_(param->notify.handle, param->notify.value,
                           param->notify.value_len);
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.conn_id != this->parent()->get_conn_id()) break;

      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Write failed (handle=0x%04X, status=0x%02X)",
                 param->write.handle, param->write.status);

        if (param->write.status == ESP_GATT_INSUF_AUTHENTICATION ||
            param->write.status == ESP_GATT_INSUF_ENCRYPTION) {
          ESP_LOGI(TAG,
                   "Write failed due to authentication; requesting pairing");
          this->ensure_paired_();
        }
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      if (!this->parent()->check_addr(param->disconnect.remote_bda)) break;

      ESP_LOGI(TAG, "Disconnected from XRS (reason=0x%02X)",
               param->disconnect.reason);
      this->tx_char_ = nullptr;
      this->notify_handles_.clear();
      this->node_state = espbt::ClientState::IDLE;
      this->publish_connection_state_(false);
      break;
    }

    default:
      break;
  }
}

void XRSRadioComponent::gap_event_handler(esp_gap_ble_cb_event_t event,
                                          esp_ble_gap_cb_param_t* param) {
  (void)event;
  (void)param;
}

// Discover characteristics and register for notifications
void XRSRadioComponent::handle_search_complete_() {
  auto service_uuid = esp32_ble::ESPBTUUID::from_raw(GME_XRS_SERVICE_STR);

  auto tx_uuid = esp32_ble::ESPBTUUID::from_raw(GME_XRS_CHAR_TX_STR);
  auto notify_main_uuid =
      esp32_ble::ESPBTUUID::from_raw(GME_XRS_CHAR_NOTIFY_MAIN_STR);
  auto notify_aux1_uuid =
      esp32_ble::ESPBTUUID::from_raw(GME_XRS_CHAR_NOTIFY_AUX1_STR);
  auto notify_aux2_uuid =
      esp32_ble::ESPBTUUID::from_raw(GME_XRS_CHAR_NOTIFY_AUX2_STR);

  // Resolve TX (command) characteristic
  this->tx_char_ = this->parent()->get_characteristic(service_uuid, tx_uuid);
  if (this->tx_char_ == nullptr) {
    ESP_LOGW(TAG, "TX char 8841 not found, falling back to main notify as TX");
    this->tx_char_ =
        this->parent()->get_characteristic(service_uuid, notify_main_uuid);
  }

  if (this->tx_char_ == nullptr) {
    ESP_LOGE(TAG, "No writable characteristic found for commands");
  } else {
    ESP_LOGI(TAG, "Using handle 0x%04X as TX characteristic",
             this->tx_char_->handle);
  }

  // Register for notifications on all three notify-capable chars
  this->notify_handles_.clear();

  auto* chr_main =
      this->parent()->get_characteristic(service_uuid, notify_main_uuid);
  auto* chr_aux1 =
      this->parent()->get_characteristic(service_uuid, notify_aux1_uuid);
  auto* chr_aux2 =
      this->parent()->get_characteristic(service_uuid, notify_aux2_uuid);

  auto register_notify = [this, &service_uuid](
                             esp32_ble_client::BLECharacteristic* chr) {
    if (chr == nullptr) return;

    const uint16_t handle = chr->handle;
    this->notify_handles_.push_back(handle);

    auto err = esp_ble_gattc_register_for_notify(
        this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
        handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to register for notify on handle 0x%04X (err=%d)",
               handle, err);
      return;
    }

    // Write CCC descriptor 0x2902 if available
    auto* descr = this->parent()->get_descriptor(
        service_uuid, chr->uuid,
        esp32_ble::ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG));

    if (descr != nullptr) {
      uint8_t notify_en[2] = {0x01, 0x00};  // notifications enabled
      auto err2 = esp_ble_gattc_write_char_descr(
          this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
          descr->handle, sizeof(notify_en), notify_en, ESP_GATT_WRITE_TYPE_RSP,
          ESP_GATT_AUTH_REQ_NONE);

      if (err2 != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write CCC descriptor for 0x%04X (err=%d)",
                 handle, err2);
      }
    } else {
      ESP_LOGW(TAG, "No CCC descriptor found for notify handle 0x%04X", handle);
    }
  };

  register_notify(chr_main);
  register_notify(chr_aux1);
  register_notify(chr_aux2);

  if (!this->notify_handles_.empty()) {
    ESP_LOGI(TAG, "Registered for %u notification handle(s)",
             static_cast<unsigned>(this->notify_handles_.size()));
  } else {
    ESP_LOGW(TAG, "No notify characteristics found on XRS service");
  }

  this->node_state = espbt::ClientState::ESTABLISHED;
  this->publish_connection_state_(true);

  // Initial handshake: echo on, verbose, and basic IDs.
  this->send_raw_command("ATE1");
  this->send_raw_command("ATV1");
  this->send_raw_command("AT+GMI?");
  this->send_raw_command("AT+GMM?");
  this->send_raw_command("AT+GMR?");
  this->send_raw_command("AT+GSN?");
  this->send_raw_command("AT+GOI?");
  this->send_raw_command("AT+WGZL"); // list all zones
  this->send_raw_command("AT+WGCHL"); // list all channels

  //  Ask for the current zone/channel so we initialise to the
  //    real radio state (this should trigger a +WGCHS: z,ch reply)
  this->send_raw_command("AT+WGCHS?");

  if (this->text_power_state_ != nullptr)
    this->text_power_state_->publish_state("Running");

  if (this->bin_power_low_ != nullptr)
    this->bin_power_low_->publish_state(false);
}

void XRSRadioComponent::handle_notify_(uint16_t handle, const uint8_t* data,
                                       uint16_t length) {
  bool known = false;
  for (auto h : this->notify_handles_) {
    if (h == handle) {
      known = true;
      break;
    }
  }

  ESP_LOGD(TAG, "Notification from handle 0x%04X (known=%s, len=%u)", handle,
           known ? "true" : "false", static_cast<unsigned>(length));

  if (length == 0 || data == nullptr) return;

  // Feed raw stream into AT parser
  this->at_parser_.feed(data, length);
}

// -----------------------------------------------------------------------------
// ATParserListener implementation
// -----------------------------------------------------------------------------

void XRSRadioComponent::on_result_code(gme_xrs_radio::ATResultCode code) {
  const char* text = (code == gme_xrs_radio::ATResultCode::OK) ? "OK" : "ERROR";
  ESP_LOGI(TAG, "AT result: %s", text);
  this->publish_status_(std::string("Result: ") + text);
}

void XRSRadioComponent::on_plus_line(const std::string& name,
                                     const std::string& payload) {
  if (payload.empty()) {
    ESP_LOGI(TAG, "AT +%s", name.c_str());
    this->publish_status_(std::string("+") + name);
  } else {
    ESP_LOGI(TAG, "AT +%s: %s", name.c_str(), payload.c_str());
    this->publish_status_(std::string("+") + name + ": " + payload);
  }

  // Device ID responses
  if (name == "GMI") {
    this->handle_plus_gmi_(payload);
    return;
  }
  if (name == "GMM") {
    this->handle_plus_gmm_(payload);
    return;
  }
  if (name == "GMR") {
    this->handle_plus_gmr_(payload);
    return;
  }
  if (name == "GSN") {
    this->handle_plus_gsn_(payload);
    return;
  }

  // Channel / zone / PTT / power notifications
  if (name == "WGCHS") {
    this->handle_plus_wgchs_(payload);
    return;
  }
  if (name == "WHZS") {
    this->handle_plus_whzs_(payload);
    return;
  }
  if (name == "WGPTT") {
    this->handle_plus_wgptt_(payload);
    return;
  }
  if (name == "WGPOW") {
    this->handle_plus_wgpow_(payload);
    return;
  }
  if (name == "WGSCAN") {
    this->handle_plus_wgscan_(payload);
    return;
  }
  if (name == "WGDUP") {
    this->handle_plus_wgdup_(payload);
    return;
  }
  if (name == "WGCSM") {
    this->handle_plus_wgcsm_(payload);
    return;
  }
  if (name == "WGSQM") {
    this->handle_plus_wgsqm_(payload);
    return;
  }
  if (name == "WGSSQ") {
    this->handle_plus_wgssq_(payload);
    return;
  }
  if (name == "WGCHSQ") {
    this->handle_plus_wgchsq_(payload);
    return;
  }
  if (name == "WGRMLOC") {
    this->handle_plus_wgrmloc_(payload);
    return;
  }
  if (name == "WGAV") {
    this->handle_plus_wgav_(payload);
    return;
  }

  // Fallback: update "last message" text sensor
  if (this->text_last_message_ != nullptr) {
    this->text_last_message_->publish_state(
        "+" + name + (payload.empty() ? "" : ": " + payload));
  }
}

void XRSRadioComponent::on_info_line(const std::string& line) {
  ESP_LOGI(TAG, "AT info: %s", line.c_str());
  this->publish_status_(line);
  if (this->text_last_message_ != nullptr) {
    this->text_last_message_->publish_state(line);
  }
}

void XRSRadioComponent::on_echo(const std::string& line) {
  ESP_LOGD(TAG, "AT echo: %s", line.c_str());
}

void XRSRadioComponent::on_unknown_line(const std::string& line) {
  ESP_LOGW(TAG, "AT unknown line: %s", line.c_str());
  this->publish_status_(line);
}

// -----------------------------------------------------------------------------
// AT "+" line handlers
// -----------------------------------------------------------------------------

// Volume report
void XRSRadioComponent::handle_plus_wgav_(const std::string& payload) {
  // payload is just "<volume>"
  int vol = strtol(payload.c_str(), nullptr, 10);
  if (vol < 0) vol = 0;
  if (vol > 31) vol = 31;

  this->current_volume_ = static_cast<uint8_t>(vol);

  if (this->number_volume_ != nullptr)
    this->number_volume_->publish_state(static_cast<float>(vol));
}

void XRSRadioComponent::handle_plus_gmi_(const std::string& payload) {
  this->manufacturer_ = payload;
  if (this->text_manufacturer_ != nullptr)
    this->text_manufacturer_->publish_state(this->manufacturer_);
}

void XRSRadioComponent::handle_plus_gmm_(const std::string& payload) {
  this->model_ = payload;
  if (this->text_model_ != nullptr)
    this->text_model_->publish_state(this->model_);
}

void XRSRadioComponent::handle_plus_gmr_(const std::string& payload) {
  this->firmware_ = payload;
  if (this->text_firmware_ != nullptr)
    this->text_firmware_->publish_state(this->firmware_);
}

void XRSRadioComponent::handle_plus_gsn_(const std::string& payload) {
  this->serial_ = payload;
  if (this->text_serial_ != nullptr)
    this->text_serial_->publish_state(this->serial_);
}

void XRSRadioComponent::handle_plus_wgchs_(const std::string& payload) {
  int zone = 0;
  int ch = 0;
  if (sscanf(payload.c_str(), "%d,%d", &zone, &ch) == 2) {
    this->current_zone_ = static_cast<uint8_t>(zone);
    this->current_channel_ = static_cast<uint8_t>(ch);

    auto label =
        this->get_channel_label_(this->current_zone_, this->current_channel_);
    if (this->text_channel_label_ != nullptr)
      this->text_channel_label_->publish_state(label);

    // keep selects in sync if present
    if (this->sel_zone_ != nullptr) this->sel_zone_->refresh_from_parent();
    if (this->sel_channel_ != nullptr)
      this->sel_channel_->refresh_from_parent();
  }
}

void XRSRadioComponent::handle_plus_whzs_(const std::string& payload) {
  int zone = 0;
  if (sscanf(payload.c_str(), "%d", &zone) == 1) {
    this->current_zone_ = static_cast<uint8_t>(zone);

    // Zone sensor is no longer exposed; keep selects in sync instead.
    if (this->sel_zone_ != nullptr) this->sel_zone_->refresh_from_parent();
    if (this->sel_channel_ != nullptr)
      this->sel_channel_->refresh_from_parent();
  }
}

void XRSRadioComponent::handle_plus_wgptt_(const std::string& payload) {
  int state = 0;
  int timer_ms = 0;
  // Payload can be "<state>" or "<state>,<timer>"
  int count = sscanf(payload.c_str(), "%d,%d", &state, &timer_ms);
  this->ptt_state_ = static_cast<uint8_t>(state);
  if (count == 2) {
    this->ptt_timer_seconds_ = timer_ms / 1000.0f;
  } else {
    this->ptt_timer_seconds_ = 0.0f;
  }

  if (this->sensor_ptt_timer_ != nullptr)
    this->sensor_ptt_timer_->publish_state(this->ptt_timer_seconds_);

  bool voice = (state == 1 || state == 2);
  bool data = (state == 2);

  if (this->bin_ptt_active_ != nullptr)
    this->bin_ptt_active_->publish_state(voice);
  if (this->bin_ptt_data_ != nullptr) this->bin_ptt_data_->publish_state(data);

  if (this->text_ptt_state_ != nullptr) {
    std::string txt;
    switch (state) {
      case 0:
        txt = "Idle";
        break;
      case 1:
        txt = "Voice";
        break;
      case 2:
        txt = "Voice+Data";
        break;
      default:
        txt = "Unknown";
        break;
    }
    this->text_ptt_state_->publish_state(txt);
  }
}

void XRSRadioComponent::handle_plus_wgpow_(const std::string& payload) {
  int state = 0;
  if (sscanf(payload.c_str(), "%d", &state) != 1) return;

  this->power_state_ = static_cast<uint8_t>(state);

  if (this->bin_power_low_ != nullptr) {
    bool low = (state == 5);  // From spec: 5 = low battery
    this->bin_power_low_->publish_state(low);
  }

  if (this->text_power_state_ != nullptr) {
    std::string txt;
    switch (state) {
      case 0:
        txt = "Booting";
        break;
      case 1:
        txt = "Running";
        break;
      case 2:
        txt = "Reset";
        break;
      case 3:
        txt = "Power down";
        break;
      case 4:
        txt = "Powered off";
        break;
      case 5:
        txt = "Low battery";
        break;
      default:
        txt = "Unknown";
        break;
    }
    this->text_power_state_->publish_state(txt);
  }
}

void XRSRadioComponent::handle_plus_wgscan_(const std::string& payload) {
  int enabled = 0;
  if (sscanf(payload.c_str(), "%d", &enabled) != 1) return;

  this->scanning_ = (enabled != 0);

  if (this->sw_scan_ != nullptr) this->sw_scan_->publish_state(this->scanning_);
}

void XRSRadioComponent::handle_plus_wgdup_(const std::string& payload) {
  int enabled = 0;
  if (sscanf(payload.c_str(), "%d", &enabled) != 1) return;

  this->duplex_enabled_ = (enabled != 0);

  if (this->sw_duplex_ != nullptr)
    this->sw_duplex_->publish_state(this->duplex_enabled_);
}

void XRSRadioComponent::handle_plus_wgcsm_(const std::string& payload) {
  int enabled = 0;
  if (sscanf(payload.c_str(), "%d", &enabled) != 1) return;

  this->silent_memory_ = (enabled != 0);

  if (this->sw_silent_memory_ != nullptr)
    this->sw_silent_memory_->publish_state(this->silent_memory_);
}

void XRSRadioComponent::handle_plus_wgsqm_(const std::string& payload) {
  int enabled = 0;
  if (sscanf(payload.c_str(), "%d", &enabled) != 1) return;

  this->quiet_memory_ = (enabled != 0);

  if (this->sw_quiet_memory_ != nullptr)
    this->sw_quiet_memory_->publish_state(this->quiet_memory_);
}

// quiet mode
void XRSRadioComponent::handle_plus_wgssq_(const std::string& payload) {
  int enabled = 0;
  if (sscanf(payload.c_str(), "%d", &enabled) != 1) return;

  this->quiet_mode_ = (enabled != 0);

  if (this->sw_quiet_mode_ != nullptr)
    this->sw_quiet_mode_->publish_state(this->quiet_mode_);
}

void XRSRadioComponent::handle_plus_wgrmloc_(const std::string& payload) {
  // Expected payload:
  // <time>,<fix>,<lat>,<lon>,"<uid>","<msg>"
  // We only care about time, lat, lon and msg; UID comes from msg pattern.

  // Parse first four comma-separated fields
  auto p1 = payload.find(',');
  if (p1 == std::string::npos) return;
  auto p2 = payload.find(',', p1 + 1);
  if (p2 == std::string::npos) return;
  auto p3 = payload.find(',', p2 + 1);
  if (p3 == std::string::npos) return;
  auto p4 = payload.find(',', p3 + 1);
  if (p4 == std::string::npos) return;

  std::string time_str = payload.substr(0, p1);
  std::string fix_str = payload.substr(p1 + 1, p2 - p1 - 1);
  std::string lat_str = payload.substr(p2 + 1, p3 - p2 - 1);
  std::string lon_str = payload.substr(p3 + 1, p4 - p3 - 1);

  // Basic numeric parsing
  (void)fix_str;  // fix not currently used
  float lat = strtof(lat_str.c_str(), nullptr);
  float lon = strtof(lon_str.c_str(), nullptr);

  // Remaining part: "uid","msg" or similar
  std::string uid_field;
  std::string msg_field;

  if (p4 + 1 < payload.size()) {
    std::string rest = payload.substr(p4 + 1);

    // Trim leading whitespace
    auto first_non = rest.find_first_not_of(" \t");
    if (first_non != std::string::npos) rest.erase(0, first_non);

    if (!rest.empty() && rest[0] == '"') {
      // UID in quotes
      auto end_uid = rest.find('"', 1);
      if (end_uid != std::string::npos) {
        uid_field = rest.substr(1, end_uid - 1);

        auto comma2 = rest.find(',', end_uid + 1);
        if (comma2 != std::string::npos) {
          std::string rest_msg = rest.substr(comma2 + 1);

          auto first_non2 = rest_msg.find_first_not_of(" \t");
          if (first_non2 != std::string::npos) rest_msg.erase(0, first_non2);

          if (!rest_msg.empty() && rest_msg[0] == '"') {
            auto end_msg = rest_msg.find('"', 1);
            if (end_msg != std::string::npos)
              msg_field = rest_msg.substr(1, end_msg - 1);
          } else {
            msg_field = rest_msg;
          }
        }
      }
    } else {
      // No quotes – treat remainder as message blob
      msg_field = rest;
    }
  }

  // Derive remote_uid and remote_message from msg_field:
  //  - If msg contains '@', use "@UID#Message" convention:
  //      remote_uid = UID
  //      remote_message = text after '#', or "" if no '#'
  //  - If there is NO '@', remote_uid stays blank and the whole fragment
  //    is dumped into remote_message (your requested behaviour).
  std::string remote_uid;
  std::string remote_message;

  if (!msg_field.empty()) {
    auto at_pos = msg_field.find('@');
    if (at_pos != std::string::npos) {
      auto hash_pos = msg_field.find('#', at_pos + 1);
      if (hash_pos != std::string::npos) {
        remote_uid = msg_field.substr(at_pos + 1, hash_pos - (at_pos + 1));
        remote_message = msg_field.substr(hash_pos + 1);
      } else {
        remote_uid = msg_field.substr(at_pos + 1);
        remote_message.clear();
      }
    } else {
      // No '@' – dump entire fragment into the remote message
      remote_message = msg_field;
    }
  }

  // Publish to sensors
  this->remote_seq_counter_++;

  if (this->sensor_remote_seq_ != nullptr)
    this->sensor_remote_seq_->publish_state(this->remote_seq_counter_);

  if (!std::isnan(lat) && this->sensor_remote_latitude_ != nullptr)
    this->sensor_remote_latitude_->publish_state(lat);

  if (!std::isnan(lon) && this->sensor_remote_longitude_ != nullptr)
    this->sensor_remote_longitude_->publish_state(lon);

  if (this->text_remote_time_ != nullptr)
    this->text_remote_time_->publish_state(time_str);

  if (this->text_remote_uid_ != nullptr)
    this->text_remote_uid_->publish_state(remote_uid);

  if (this->text_remote_message_ != nullptr)
    this->text_remote_message_->publish_state(remote_message);
}

void XRSRadioComponent::handle_plus_wgchsq_(const std::string& payload) {
  // Heuristic: "<zone>,<channel>,\"<name>\",..." – we only care about first 3
  // fields.
  int zone = 0;
  int ch = 0;
  char name_buf[64] = {0};

  int matched =
      sscanf(payload.c_str(), "%d,%d,\"%63[^\"]", &zone, &ch, name_buf);
  if (matched >= 2) {
    ChannelInfo info;
    info.zone = static_cast<uint8_t>(zone);
    info.channel = static_cast<uint8_t>(ch);
    if (matched == 3) {
      info.name = name_buf;
    } else {
      info.name.clear();
    }

    // Replace or append
    bool replaced = false;
    for (auto& existing : this->channel_table_) {
      if (existing.zone == info.zone && existing.channel == info.channel) {
        existing = info;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      this->channel_table_.push_back(info);
    }

    // Update channel label if this is the current channel.
    if (info.zone == this->current_zone_ &&
        info.channel == this->current_channel_) {
      if (this->text_channel_label_ != nullptr) {
        this->text_channel_label_->publish_state(
            this->get_channel_label_(info.zone, info.channel));
      }
    }

    // Refresh select options and selected values
    if (this->sel_zone_ != nullptr) {
      this->sel_zone_->refresh_from_parent();
    }
    if (this->sel_channel_ != nullptr) {
      this->sel_channel_->refresh_from_parent();
    }
  }
}

// -----------------------------------------------------------------------------
// Public control API (called from wrapper entities)
// -----------------------------------------------------------------------------

void XRSRadioComponent::set_volume(uint8_t volume) {
  // Clamp if you want, e.g. 0–31
  if (volume > 31) volume = 31;

  char cmd[24];
  // XRS uses WGAV for audio volume
  snprintf(cmd, sizeof(cmd), "AT+WGAV=%u", static_cast<unsigned>(volume));
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_location_mode(bool enabled) {
  this->location_mode_ = enabled;
  if (this->sw_location_mode_ != nullptr)
    this->sw_location_mode_->publish_state(enabled);
}

void XRSRadioComponent::set_scan_enabled(bool enabled) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGSCAN=%d", enabled ? 1 : 0);
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_duplex_enabled(bool enabled) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGDUP=%d", enabled ? 1 : 0);
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_quiet_mode(bool enabled) {
  // Quiet mode using WGSSQ=<mode>,<force>; we do not force quiet on non-quiet
  // channels here.
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGSSQ=%d,0", enabled ? 1 : 0);
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_quiet_memory(bool enabled) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGSQM=%d", enabled ? 1 : 0);
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_silent_memory(bool enabled) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGCSM=%d", enabled ? 1 : 0);
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_zone(uint8_t zone) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGZS=%u", static_cast<unsigned>(zone));
  this->send_raw_command(cmd);
}

void XRSRadioComponent::set_channel(uint8_t zone, uint8_t channel) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+WGCHS=%u,%u", static_cast<unsigned>(zone),
           static_cast<unsigned>(channel));
  this->send_raw_command(cmd);
}

// Build list of "Z1", "Z2", ... from channel table.
std::vector<std::string> XRSRadioComponent::get_zone_options() const {
  std::vector<std::string> out;
  for (const auto& info : this->channel_table_) {
    char buf[8];
    snprintf(buf, sizeof(buf), "Z%u", static_cast<unsigned>(info.zone));
    std::string label = buf;
    bool exists = false;
    for (const auto& existing : out) {
      if (existing == label) {
        exists = true;
        break;
      }
    }
    if (!exists) out.push_back(label);
  }
  return out;
}

// Build list of channel numbers for the current zone only.
std::vector<std::string> XRSRadioComponent::get_channel_options() const {
  std::vector<std::string> out;
  uint8_t zone = this->current_zone_;

  // Simple insertion de-dup; channel_table_ is expected to be small.
  for (const auto& info : this->channel_table_) {
    if (info.zone != zone) continue;

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(info.channel));
    std::string label = buf;

    bool exists = false;
    for (const auto& existing : out) {
      if (existing == label) {
        exists = true;
        break;
      }
    }
    if (!exists) out.push_back(label);
  }
  return out;
}
// -----------------------------------------------------------------------------
// Low-level helpers
// -----------------------------------------------------------------------------

bool XRSRadioComponent::is_client_ready_() {
  if (this->parent() == nullptr) return false;
  if (this->node_state != espbt::ClientState::ESTABLISHED) return false;
  if (this->tx_char_ == nullptr) return false;
  return true;
}

void XRSRadioComponent::ensure_paired_() {
  if (this->parent() == nullptr) return;
  if (this->parent()->is_paired()) return;

  auto err = this->parent()->pair();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to initiate pairing/encryption (err=%d)", err);
  } else {
    ESP_LOGI(TAG, "Pairing/encryption requested");
  }
}

void XRSRadioComponent::send_raw_command(const std::string& cmd) {
  if (!this->is_client_ready_()) {
    ESP_LOGW(TAG, "Cannot send command, BLE client not ready");
    return;
  }

  if (cmd.empty()) return;

  std::string line = cmd;
  if (line.back() != '\n') {
    if (line.back() != '\r') line.push_back('\r');
    line.push_back('\n');
  }

  std::vector<uint8_t> buffer(line.begin(), line.end());

  auto err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
      this->tx_char_->handle, static_cast<uint16_t>(buffer.size()),
      buffer.data(), ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed (err=%d)", err);
  } else {
    ESP_LOGD(TAG, "TX (%u bytes): %s", static_cast<unsigned>(buffer.size()),
             cmd.c_str());
  }
}

void XRSRadioComponent::publish_status_(const std::string& line) {
  if (this->status_text_sensor_ != nullptr) {
    this->status_text_sensor_->publish_state(line);
  }
}

void XRSRadioComponent::publish_connection_state_(bool connected) {
  this->connected_ = connected;
  if (this->bin_connected_ != nullptr)
    this->bin_connected_->publish_state(connected);
}

void XRSRadioComponent::publish_all_state_() {
  // Re-publish key state to entities that care.

  if (this->number_volume_ != nullptr)
    this->number_volume_->publish_state(this->current_volume_);

  // Switch-based booleans
  if (this->sw_scan_ != nullptr) this->sw_scan_->publish_state(this->scanning_);
  if (this->sw_duplex_ != nullptr)
    this->sw_duplex_->publish_state(this->duplex_enabled_);
  if (this->sw_silent_memory_ != nullptr)
    this->sw_silent_memory_->publish_state(this->silent_memory_);
  if (this->sw_quiet_memory_ != nullptr)
    this->sw_quiet_memory_->publish_state(this->quiet_memory_);
  if (this->sw_quiet_mode_ != nullptr)
    this->sw_quiet_mode_->publish_state(this->quiet_mode_);

  auto label =
      this->get_channel_label_(this->current_zone_, this->current_channel_);
  if (this->text_channel_label_ != nullptr)
    this->text_channel_label_->publish_state(label);
}

// Resolve a human-readable channel label for the given zone/channel.
std::string XRSRadioComponent::get_channel_label_(uint8_t zone,
                                                  uint8_t channel) const {
  for (const auto& info : this->channel_table_) {
    if (info.zone == zone && info.channel == channel) {
      if (!info.name.empty()) return info.name;
      break;
    }
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "Z%u / Ch %u", static_cast<unsigned>(zone),
           static_cast<unsigned>(channel));
  return std::string(buf);
}

// -----------------------------------------------------------------------------
// Location upload
// -----------------------------------------------------------------------------

void XRSRadioComponent::send_location_update_() {
  if (!this->location_mode_) return;

  if (this->latitude_sensor_ == nullptr || this->longitude_sensor_ == nullptr)
    return;

  if (!this->latitude_sensor_->has_state() ||
      !this->longitude_sensor_->has_state())
    return;

  float lat = this->latitude_sensor_->state;
  float lon = this->longitude_sensor_->state;

  if (std::isnan(lat) || std::isnan(lon)) return;

  // Time parameter: we don't have RTC integration here, so send "000000" (UTC).
  char cmd[96];
  snprintf(cmd, sizeof(cmd), "AT+WGTLOC=000000,%.6f,%.6f", lat, lon);
  this->send_raw_command(cmd);
}

void XRSRadioComponent::send_location_with_message() {
  // Manual one-shot send for the button: message (if any) + WGTLOC.

  if (!this->is_client_ready_()) {
    ESP_LOGW(TAG, "Cannot send location+message: BLE client not ready");
    return;
  }

  if (this->latitude_sensor_ == nullptr || this->longitude_sensor_ == nullptr)
    return;

  if (!this->latitude_sensor_->has_state() ||
      !this->longitude_sensor_->has_state())
    return;

  float lat = this->latitude_sensor_->state;
  float lon = this->longitude_sensor_->state;

  if (std::isnan(lat) || std::isnan(lon)) return;

  // Optional message from bound text sensor
  std::string msg;
  if (this->message_sensor_ != nullptr && this->message_sensor_->has_state()) {
    msg = this->message_sensor_->state.c_str();
  }

  if (!msg.empty()) {
    // Escape quotes/backslashes for safety
    std::string escaped;
    escaped.reserve(msg.size());
    for (char c : msg) {
      if (c == '"' || c == '\\') escaped.push_back('\\');
      escaped.push_back(c);
    }

    char cmd_msg[160];
    snprintf(cmd_msg, sizeof(cmd_msg), "AT+WGTMSG=\"%s\"", escaped.c_str());
    this->send_raw_command(cmd_msg);
  }

  // Time parameter: still "000000" (no RTC)
  char cmd_loc[96];
  snprintf(cmd_loc, sizeof(cmd_loc), "AT+WGTLOC=000000,%.6f,%.6f", lat, lon);
  this->send_raw_command(cmd_loc);
}

}  // namespace gme_xrs_radio
}  // namespace esphome
