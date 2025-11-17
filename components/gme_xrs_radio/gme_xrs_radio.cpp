#include "gme_xrs_radio.h"

#include <esp_gattc_api.h>
#include <esp_gatt_defs.h>
#include "esp_gap_ble_api.h"
#include "esp_err.h"

namespace esphome {
namespace gme_xrs_radio {

static const char *const TAG = "gme_xrs_radio";

// GME XRS primary service UUID
static const char *const GME_XRS_SERVICE_STR = "49535343-fe7d-4ae5-8fa9-9fafd205e455";

// Candidate characteristic UUIDs (from nRF Connect dump)
// TX (write)
static const char *const GME_XRS_CHAR_TX_STR = "49535343-8841-43f4-a8d4-ecbe34729bb3";

// Notify/stream characteristics
static const char *const GME_XRS_CHAR_NOTIFY_MAIN_STR = "49535343-1e4d-4bd9-ba61-23c647249616"; // TX
static const char *const GME_XRS_CHAR_NOTIFY_AUX1_STR = "49535343-aca3-481c-91ec-d85e28a60318";
static const char *const GME_XRS_CHAR_NOTIFY_AUX2_STR = "49535343-026e-3a9b-954c-97daef17e26e";


// 49535343-8841-43F4-A8D4-ECBE34729BB3 RX


// Configure BLE security to match the GME XRS / Microchip expectations.
//
// - LE Secure Connections ONLY
// - MITM protection (numeric comparison)
// - Bonding enabled
// - 16-byte key size
// - Exchange ENC + ID keys, no OOB
static void configure_gme_xrs_security() {
  // Auth requirement: SC + MITM + Bonding
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;

  // 16-byte key (max for BLE)
  uint8_t key_size = 16;

  // Exchange encryption and identity keys both ways
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t resp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

  // No out-of-band data
  uint8_t oob_enable = ESP_BLE_OOB_DISABLE;

  esp_err_t err;

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,
                                       &auth_req, sizeof(auth_req));
  ESP_LOGI(TAG, "GME XRS: set AUTHEN_REQ_MODE=0x%02X -> %s",
           auth_req, esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,
                                       &key_size, sizeof(key_size));
  ESP_LOGI(TAG, "GME XRS: set MAX_KEY_SIZE=%u -> %s",
           key_size, esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,
                                       &init_key, sizeof(init_key));
  ESP_LOGI(TAG, "GME XRS: set INIT_KEY mask=0x%02X -> %s",
           init_key, esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,
                                       &resp_key, sizeof(resp_key));
  ESP_LOGI(TAG, "GME XRS: set RSP_KEY mask=0x%02X -> %s",
           resp_key, esp_err_to_name(err));

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT,
                                       &oob_enable, sizeof(oob_enable));
  ESP_LOGI(TAG, "GME XRS: set OOB_SUPPORT=%u -> %s",
           oob_enable, esp_err_to_name(err));
}


void GmeXrsRadioComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GME XRS radio (BLE)...");

  // Tighten BLE security for GME XRS before any connections occur.
  configure_gme_xrs_security();

  // BLEClientNode::node_state is used by ble_client to know when we're ready.
  this->node_state = espbt::ClientState::INIT;
}

void GmeXrsRadioComponent::loop() {
  // No periodic traffic required; ble_client handles connection state and retries.
}

void GmeXrsRadioComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GME XRS Radio (BLE) v854");
  ESP_LOGCONFIG(TAG, "  Service UUID: %s", GME_XRS_SERVICE_STR);
  if (this->parent() != nullptr) {
    ESP_LOGCONFIG(TAG, "  Bound to BLE client index: %u", this->parent()->get_connection_index());
  }
}

void GmeXrsRadioComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                               esp_ble_gattc_cb_param_t *param) {
  (void) gattc_if;

  if (this->parent() == nullptr) {
    return;
  }

  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.conn_id != this->parent()->get_conn_id())
        break;

      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "GATT connection opened (conn_id=%d)", param->open.conn_id);
        this->ensure_paired_();
      } else {
        ESP_LOGW(TAG, "Failed to open GATT connection, status=0x%02X", param->open.status);
      }
      break;
    }

    case ESP_GATTC_ENC_CMPL_CB_EVT: {
      // We get this when link encryption / key negotiation has completed.
      // The BLE client layer already logs "auth complete" + reason,
      // but this gives us a component-level hook so we can correlate it.
      ESP_LOGI(TAG,
               "GATT ENC_CMPL event received (encryption / security completed on XRS link)");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->parent()->get_conn_id())
        break;

      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service discovery failed, status=0x%02X", param->search_cmpl.status);
        break;
      }

      ESP_LOGI(TAG, "Service discovery complete, configuring XRS characteristics...");
      this->handle_search_complete_();
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id())
        break;

      this->handle_notify_(param->notify.handle, param->notify.value, param->notify.value_len);
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.conn_id != this->parent()->get_conn_id())
        break;

      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Write failed (handle=0x%04X, status=0x%02X)",
                 param->write.handle, param->write.status);

        if (param->write.status == ESP_GATT_INSUF_AUTHENTICATION ||
            param->write.status == ESP_GATT_INSUF_ENCRYPTION) {
          ESP_LOGI(TAG, "Write failed due to authentication; requesting pairing/encryption");
          this->ensure_paired_();
        }
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      if (!this->parent()->check_addr(param->disconnect.remote_bda))
        break;

      ESP_LOGI(TAG, "Disconnected from XRS (reason=0x%02X)", param->disconnect.reason);
      this->tx_char_ = nullptr;
      this->notify_handles_.clear();
      this->node_state = this->node_state = espbt::ClientState::IDLE;
      break;
    }

    default:
      break;
  }
}

void GmeXrsRadioComponent::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  // For now we rely on the global BLE client security handling.
  // This hook is available if you want to customise passkey / NC behaviour later.
  (void) event;
  (void) param;
}

void GmeXrsRadioComponent::handle_search_complete_() {
  using esp32_ble::ESPBTUUID;

  auto service_uuid = ESPBTUUID::from_raw(GME_XRS_SERVICE_STR);

  auto tx_uuid = ESPBTUUID::from_raw(GME_XRS_CHAR_TX_STR);
  auto notify_main_uuid = ESPBTUUID::from_raw(GME_XRS_CHAR_NOTIFY_MAIN_STR);
  auto notify_aux1_uuid = ESPBTUUID::from_raw(GME_XRS_CHAR_NOTIFY_AUX1_STR);
  auto notify_aux2_uuid = ESPBTUUID::from_raw(GME_XRS_CHAR_NOTIFY_AUX2_STR);

  // Resolve TX (command) characteristic
  this->tx_char_ = this->parent()->get_characteristic(service_uuid, tx_uuid);
  if (this->tx_char_ == nullptr) {
    ESP_LOGW(TAG, "TX char 8841 not found, falling back to notify_main as TX");
    this->tx_char_ = this->parent()->get_characteristic(service_uuid, notify_main_uuid);
  }

  if (this->tx_char_ == nullptr) {
    ESP_LOGE(TAG, "No writable characteristic found for commands");
  } else {
    ESP_LOGI(TAG, "Using handle 0x%04X as TX characteristic", this->tx_char_->handle);
  }

  // Register for notifications on all three notify-capable chars
  this->notify_handles_.clear();

  auto *chr_main = this->parent()->get_characteristic(service_uuid, notify_main_uuid);
  auto *chr_aux1 = this->parent()->get_characteristic(service_uuid, notify_aux1_uuid);
  auto *chr_aux2 = this->parent()->get_characteristic(service_uuid, notify_aux2_uuid);

  auto register_notify = [this, &service_uuid](esp32_ble_client::BLECharacteristic *chr) {
    if (chr == nullptr)
      return;

    const uint16_t handle = chr->handle;
    this->notify_handles_.push_back(handle);

    auto err = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                 this->parent()->get_remote_bda(), handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to register for notify on handle 0x%04X (err=%d)", handle, err);
      return;
    }

    // Write CCC descriptor (0x2902) if available
    auto *descr = this->parent()->get_descriptor(
        service_uuid,
        chr->uuid,
        esp32_ble::ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG));

    if (descr != nullptr) {
      uint8_t notify_en[2] = {0x01, 0x00};  // notifications enabled
      auto err2 = esp_ble_gattc_write_char_descr(
          this->parent()->get_gattc_if(),
          this->parent()->get_conn_id(),
          descr->handle,
          sizeof(notify_en),
          notify_en,
          ESP_GATT_WRITE_TYPE_RSP,
          ESP_GATT_AUTH_REQ_NONE);

      if (err2 != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write CCC descriptor for 0x%04X (err=%d)", handle, err2);
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
}

void GmeXrsRadioComponent::handle_notify_(uint16_t handle, const uint8_t *data, uint16_t length) {
  bool known = false;
  for (auto h : this->notify_handles_) {
    if (h == handle) {
      known = true;
      break;
    }
  }

  ESP_LOGD(TAG, "Notification from handle 0x%04X (known=%s, len=%u)",
           handle, known ? "true" : "false", static_cast<unsigned>(length));

  if (length == 0 || data == nullptr)
    return;

  // Append to ASCII line buffer and process full lines
  this->rx_buffer_.append(reinterpret_cast<const char *>(data), length);
  this->process_rx_buffer_();
}

void GmeXrsRadioComponent::process_rx_buffer_() {
  std::size_t pos;
  while ((pos = this->rx_buffer_.find('\n')) != std::string::npos) {
    std::string line = this->rx_buffer_.substr(0, pos);
    this->rx_buffer_.erase(0, pos + 1);

    // Trim CR
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line.empty())
      continue;

    this->handle_line_(line);
  }
}

void GmeXrsRadioComponent::handle_line_(const std::string &line) {
  ESP_LOGI(TAG, "RX: %s", line.c_str());
  this->publish_status_(line);

  // Minimal example parsing for +WGCHS: 1,41 messages
  // (head 1, channel 41, etc.)
  if (line.rfind("+WGCHS:", 0) == 0) {
    int head = 0;
    int channel = 0;
    if (sscanf(line.c_str(), "+WGCHS: %d,%d", &head, &channel) == 2) {
      ESP_LOGI(TAG, "Parsed WGCHS: head=%d channel=%d", head, channel);
      // Hook for later: update channel select / sensor when you add them.
    }
  }

  // You can extend this with your existing XRS parser here.
}

void GmeXrsRadioComponent::publish_status_(const std::string &line) {
  if (this->status_text_sensor_ != nullptr) {
    this->status_text_sensor_->publish_state(line);
  }
}

bool GmeXrsRadioComponent::is_client_ready_() {
  if (this->parent() == nullptr)
    return false;
  if (this->node_state != espbt::ClientState::ESTABLISHED)
    return false;
  if (this->tx_char_ == nullptr)
    return false;
  return true;
}

void GmeXrsRadioComponent::ensure_paired_() {
  if (this->parent() == nullptr)
    return;
  if (this->parent()->is_paired())
    return;

  auto err = this->parent()->pair();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to initiate pairing/encryption (err=%d)", err);
  } else {
    ESP_LOGI(TAG, "Pairing/encryption requested");
  }
}

void GmeXrsRadioComponent::send_raw_command(const std::string &cmd) {
  if (!this->is_client_ready_()) {
    ESP_LOGW(TAG, "Cannot send command, BLE client not ready");
    return;
  }

  if (cmd.empty())
    return;

  // Ensure CRLF terminator
  std::string line = cmd;
  if (line.back() != '\n') {
    if (line.back() != '\r')
      line.push_back('\r');
    line.push_back('\n');
  }

  std::vector<uint8_t> buffer(line.begin(), line.end());

  auto err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(),
      this->parent()->get_conn_id(),
      this->tx_char_->handle,
      static_cast<uint16_t>(buffer.size()),
      buffer.data(),
      ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed (err=%d)", err);
  } else {
    ESP_LOGD(TAG, "TX (%u bytes): %s",
             static_cast<unsigned>(buffer.size()), cmd.c_str());
  }
}

}  // namespace gme_xrs_radio
}  // namespace esphome
