#include "xrs_switch.h"

#include "esphome/core/log.h"
#include "../xrs_radio.h"

namespace esphome {
namespace gme_xrs_radio {

static const char *const TAG = "gme_xrs_radio.switch";

void XRSRadioSwitch::write_state(bool state) {
  if (parent_ == nullptr) {
    ESP_LOGW(TAG, "Switch has no parent, ignoring %d", state);
    return;
  }

  switch (type_) {
    case XRS_SWITCH_LOCATION_MODE:
      parent_->set_location_mode(state);
      break;
    case XRS_SWITCH_SCAN:
      parent_->set_scan_enabled(state);
      break;
    case XRS_SWITCH_DUPLEX:
      parent_->set_duplex_enabled(state);
      break;
    case XRS_SWITCH_QUIET_MODE:
      parent_->set_quiet_mode(state);
      break;
    case XRS_SWITCH_QUIET_MEMORY:
      parent_->set_quiet_memory(state);
      break;
    case XRS_SWITCH_SILENT_MEMORY:
      parent_->set_silent_memory(state);
      break;
    default:
      ESP_LOGW(TAG, "Unhandled XRSRadioSwitch type %d",
               static_cast<int>(type_));
      break;
  }

  // reflect state in HA
  this->publish_state(state);
}

}  // namespace gme_xrs_radio
}  // namespace esphome
