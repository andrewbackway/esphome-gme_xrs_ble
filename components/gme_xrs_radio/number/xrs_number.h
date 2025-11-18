#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace gme_xrs_radio {

class XRSRadioComponent;

// Forward-declare your existing enum
enum XRSNumberType : uint8_t;

class XRSRadioNumber : public number::Number, public Component {
 public:
  void set_parent(XRSRadioComponent *parent) { parent_ = parent; }
  void set_type(XRSNumberType type) { type_ = type; }

  void setup() override {}
  void dump_config() override {}

 protected:
  void control(float value) override;  // implement sending to radio

  XRSRadioComponent *parent_{nullptr};
  XRSNumberType type_{};
};

}  // namespace gme_xrs_radio
}  // namespace esphome
