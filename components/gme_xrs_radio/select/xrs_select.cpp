#include "xrs_select.h"

#include <cstdio>

#include "esphome/core/log.h"
#include "../xrs_radio.h"  // for XRSRadioComponent, XRSSelectType and helpers

namespace esphome {
namespace gme_xrs_radio {

static const char *const TAG = "xrs_radio.select";

void XRSRadioSelect::setup() {
  // Populate initial options from the hub
  this->update_options_();
}

void XRSRadioSelect::dump_config() {
  ESP_LOGCONFIG(TAG, "XRS Radio Select");
}

select::SelectTraits XRSRadioSelect::get_traits() {
  // Start from base traits so we keep defaults (optimistic, etc.)
  auto traits = select::Select::get_traits();
  traits.set_options(this->options_);
  return traits;
}

void XRSRadioSelect::update_options_() {
  this->options_.clear();

  if (this->parent_ == nullptr) {
    return;
  }

  switch (this->type_) {
    case XRSSelectType::XRS_SELECT_ZONE: {
      auto zones = this->parent_->get_zone_options();
      this->options_.assign(zones.begin(), zones.end());
      break;
    }
    case XRSSelectType::XRS_SELECT_CHANNEL: {
      auto chans = this->parent_->get_channel_options();
      this->options_.assign(chans.begin(), chans.end());
      break;
    }
    default:
      break;
  }
}

void XRSRadioSelect::refresh_from_parent() {
  if (this->parent_ == nullptr) {
    return;
  }

  // Refresh the options list first
  this->update_options_();

  // Now publish the current value from the hub
  char buf[32];

  switch (this->type_) {
    case XRSSelectType::XRS_SELECT_ZONE: {
      unsigned zone = this->parent_->get_current_zone();
      std::snprintf(buf, sizeof(buf), "Z%u", zone);
      this->publish_state(buf);
      break;
    }

    case XRSSelectType::XRS_SELECT_CHANNEL: {
      unsigned zone = this->parent_->get_current_zone();
      unsigned ch = this->parent_->get_current_channel();
      std::snprintf(buf, sizeof(buf), "Z%u / Ch %u", zone, ch);
      this->publish_state(buf);
      break;
    }

    default:
      break;
  }
}

void XRSRadioSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Select has no parent, ignoring selection '%s'",
             value.c_str());
    return;
  }

  ESP_LOGD(TAG, "User selected '%s'", value.c_str());

  if (this->type_ == XRSSelectType::XRS_SELECT_ZONE) {
    // Expect values like "Z1", "Z2", ...
    unsigned zone = 0;
    if (std::sscanf(value.c_str(), "Z%u", &zone) == 1) {
      this->parent_->set_zone(static_cast<uint8_t>(zone));
      this->publish_state(value);
    } else {
      ESP_LOGW(TAG, "Failed to parse zone from '%s'", value.c_str());
    }
  } else {
    // Expect values like "Z1 / Ch 12"
    unsigned zone = 0;
    unsigned ch = 0;
    if (std::sscanf(value.c_str(), "Z%u / Ch %u", &zone, &ch) == 2) {
      this->parent_->set_channel(static_cast<uint8_t>(zone),
                                 static_cast<uint8_t>(ch));
      this->publish_state(value);
    } else {
      ESP_LOGW(TAG, "Failed to parse channel from '%s'", value.c_str());
    }
  }
}

}  // namespace xrs_radio
}  // namespace esphome
