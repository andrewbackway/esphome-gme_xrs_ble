#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace gme_xrs_radio {

class XRSRadioComponent;
enum XRSSwitchType : uint8_t;

class XRSRadioSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(XRSRadioComponent *parent) { parent_ = parent; }
  void set_type(XRSSwitchType type) { type_ = type; }

  void setup() override {}
  void dump_config() override {}

 protected:
  void write_state(bool state) override;

  XRSRadioComponent *parent_{nullptr};
  XRSSwitchType type_{};
};

}  // namespace gme_xrs_radio
}  // namespace esphome
