#pragma once

#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace gme_xrs_radio {

class XRSRadioComponent;
enum class XRSSelectType: uint8_t; // forward declaration of the enum from xrs_radio.h

class XRSRadioSelect : public select::Select, public Component {
 public:
  void set_parent(XRSRadioComponent *parent) { this->parent_ = parent; }
  void set_type(XRSSelectType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

  // Called by the hub when zone/channel state or channel table changes
  void refresh_from_parent();

 protected:
  // Called when HA/user changes the selected option
  void control(const std::string &value) override;

  // Internal: refresh options_ from the hub’s zone/channel table
  void update_options_();

  XRSRadioComponent *parent_{nullptr};
  XRSSelectType type_;
  std::vector<std::string> options_;
};

}  // namespace gme_xrs_radio
}  // namespace esphome
