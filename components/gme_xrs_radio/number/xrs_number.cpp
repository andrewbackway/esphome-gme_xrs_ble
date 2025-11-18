#include "xrs_number.h"

#include "esphome/core/log.h"
#include "../xrs_radio.h"

namespace esphome {
namespace gme_xrs_radio {

static const char *const TAG = "gme_xrs_radio.number";

void XRSRadioNumber::control(float value) {
  if (parent_ == nullptr) {
    ESP_LOGW(TAG, "Number has no parent, ignoring %.1f", value);
    return;
  }

  switch (type_) {
    case XRS_NUMBER_VOLUME: {
      int vol = static_cast<int>(value + 0.5f);
      if (vol < 0)
        vol = 0;
      if (vol > 31)
        vol = 31;
      parent_->set_volume(static_cast<uint8_t>(vol));
      this->publish_state(vol);
      break;
    }

    default:
      ESP_LOGW(TAG, "Unhandled XRSRadioNumber type %d",
               static_cast<int>(type_));
      break;
  }
}

}  // namespace gme_xrs_radio
}  // namespace esphome
