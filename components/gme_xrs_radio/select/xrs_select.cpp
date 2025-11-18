#include "xrs_select.h"

#include "../xrs_radio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gme_xrs_radio {

static const char* const TAG = "gme_xrs_radio.select";

void XRSRadioSelect::setup() {
  // Initialize options so the entity in Home Assistant isn't empty on boot
  this->update_options_();
}

void XRSRadioSelect::dump_config() {
  LOG_SELECT("", "GME XRS Select", this);
  ESP_LOGCONFIG(TAG, "  Select Type: %s",
                this->type_ == XRS_SELECT_ZONE ? "ZONE" : "CHANNEL");
}

void XRSRadioSelect::update_options_() {
  if (this->parent_ == nullptr) return;

  this->options_.clear();
  std::vector<std::string> opts;

  switch (this->type_) {
    case XRS_SELECT_ZONE: {
      // Try to build from channel table
      opts = this->parent_->get_zone_options();

      // Fallback in case channel table is still empty: just synthesise Z1
      if (opts.empty()) {
        ESP_LOGW(TAG, "Zone select: zone table empty, using fallback");
        // Z1..Z8
        for (uint8_t z = 1; z <= 8; z++) {
          char buf[8];
          snprintf(buf, sizeof(buf), "Z%u", static_cast<unsigned>(z));
          opts.emplace_back(buf);
        }
      }
      break;
    }

    case XRS_SELECT_CHANNEL: {
      // Channels for current zone only
      opts = this->parent_->get_channel_options();

      // Fallback: generic 1..80 channel list
      if (opts.empty()) {
        ESP_LOGW(TAG, "Channel select: channel table empty, using fallback");
        for (uint8_t ch = 1; ch <= 80; ch++) {
          char buf[8];
          snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(ch));
          opts.emplace_back(buf);
        }
        break;
      }

      default:
        break;
    }
  }
  if (opts.empty()) {
    ESP_LOGW(TAG, "Select (%s): still no options after update",
             this->type_ == XRS_SELECT_ZONE ? "zone" : "channel");
    return;
  }

  // Store and expose to the base class / API
  this->options_ = std::move(opts);
  this->traits.set_options(this->options_);
}

void XRSRadioSelect::refresh_from_parent() {
  if (this->parent_ == nullptr) return;

  // Rebuild options from the channel table / current state
  this->update_options_();

  char buf[32] = {0};

  switch (this->type_) {
    case XRS_SELECT_ZONE: {
      unsigned zone = this->parent_->get_current_zone();
      std::snprintf(buf, sizeof(buf), "Z%u", zone);
      this->publish_state(buf);
      break;
    }

    case XRS_SELECT_CHANNEL: {
      unsigned ch = this->parent_->get_current_channel();
      std::snprintf(buf, sizeof(buf), "%u", ch);
      this->publish_state(buf);
      break;
    }

    default:
      break;
  }
}

void XRSRadioSelect::control(const std::string& value) {
  if (this->parent_ == nullptr) return;

  // Value is already validated against traits/options by the base class

  if (this->type_ == XRS_SELECT_ZONE) {
    // Value looks like "Z1", "Z2", ...
    unsigned zone = 0;
    if (std::sscanf(value.c_str(), "Z%u", &zone) == 1) {
      this->parent_->set_zone(static_cast<uint8_t>(zone));
      this->publish_state(value);
    } else {
      ESP_LOGW(TAG, "Zone select: could not parse '%s'", value.c_str());
    }
  } else if (this->type_ == XRS_SELECT_CHANNEL) {
    // Value is the channel number within the current zone, e.g. "46".
    unsigned ch = 0;
    if (std::sscanf(value.c_str(), "%u", &ch) == 1) {
      unsigned zone = this->parent_->get_current_zone();
      this->parent_->set_channel(static_cast<uint8_t>(zone),
                                 static_cast<uint8_t>(ch));
      this->publish_state(value);
    } else {
      ESP_LOGW(TAG, "Channel select: could not parse '%s'", value.c_str());
    }
  }
}

}  // namespace gme_xrs_radio
}  // namespace esphome
