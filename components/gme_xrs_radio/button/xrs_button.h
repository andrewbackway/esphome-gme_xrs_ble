#pragma once

#ifdef USE_ESP32

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "../xrs_radio.h"

namespace esphome {
namespace gme_xrs_radio {

class XRSRadioButton : public button::Button, public Component {
 public:
  void set_parent(XRSRadioComponent* parent) { parent_ = parent; }

 protected:
  void press_action() override {
    if (this->parent_ != nullptr) {
      this->parent_->send_location_with_message();
    }
  }

  XRSRadioComponent* parent_{nullptr};
};

}  // namespace gme_xrs_radio
}  // namespace esphome

#endif  // USE_ESP32
