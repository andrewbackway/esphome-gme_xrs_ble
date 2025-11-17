#include "xrs_at_parser.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gme_xrs_radio {

static const char *const TAG = "gme_xrs_radio.at";

ATParser::ATParser(ATParserListener *listener) : listener_(listener) {}

void ATParser::reset() { this->buffer_.clear(); }

void ATParser::feed(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    this->feed(static_cast<char>(data[i]));
  }
}

void ATParser::feed(char c) {
  // Treat both CR and LF as line terminators.
  if (c == '\r' || c == '\n') {
    if (!this->buffer_.empty()) {
      std::string line = this->buffer_;
      this->buffer_.clear();
      this->handle_complete_line_(line);
    }
    // If buffer was empty, ignore stray CR/LF.
    return;
  }

  // Normal character, append if within limit.
  if (this->buffer_.size() >= MAX_LINE_LENGTH) {
    ESP_LOGW(TAG, "AT line too long (%u >= %u), dropping line",
             (unsigned) this->buffer_.size(), (unsigned) MAX_LINE_LENGTH);
    this->buffer_.clear();
  }

  this->buffer_.push_back(c);
}

void ATParser::trim_whitespace_(std::string &s) {
  // Trim leading whitespace.
  size_t start = 0;
  while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
    start++;
  }

  // Trim trailing whitespace.
  size_t end = s.size();
  while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
    end--;
  }

  if (start == 0 && end == s.size()) {
    return;  // nothing to trim
  }

  s = s.substr(start, end - start);
}

void ATParser::handle_complete_line_(std::string &line) {
  // Normalize whitespace
  trim_whitespace_(line);

  if (line.empty()) {
    return;
  }

  ESP_LOGV(TAG, "RX line: '%s'", line.c_str());

  // 1. Result codes
  if (line == "OK") {
    if (this->listener_ != nullptr) {
      this->listener_->on_result_code(ATResultCode::OK);
    }
    return;
  }

  if (line == "ERROR") {
    if (this->listener_ != nullptr) {
      this->listener_->on_result_code(ATResultCode::ERROR);
    }
    return;
  }

  // 2. Echoed commands (ATE1)
  // Very simple heuristic: line starts with "AT".
  if (line.size() >= 2 && line[0] == 'A' && line[1] == 'T') {
    if (this->listener_ != nullptr) {
      this->listener_->on_echo(line);
    } else {
      ESP_LOGV(TAG, "Echo (ignored): '%s'", line.c_str());
    }
    return;
  }

  // 3. Lines starting with "+" => notifications or verbose responses
  if (!line.empty() && line[0] == '+') {
    std::string without_plus = line.substr(1);  // remove leading '+'
    trim_whitespace_(without_plus);

    std::string name;
    std::string payload;

    auto colon_pos = without_plus.find(':');
    if (colon_pos == std::string::npos) {
      // "+CMD" form, no explicit payload
      name = without_plus;
      payload.clear();
    } else {
      name = without_plus.substr(0, colon_pos);
      payload = without_plus.substr(colon_pos + 1);
      trim_whitespace_(name);
      trim_whitespace_(payload);
    }

    if (this->listener_ != nullptr) {
      this->listener_->on_plus_line(name, payload);
    } else {
      ESP_LOGV(TAG, "Plus line (no listener): name='%s' payload='%s'",
               name.c_str(), payload.c_str());
    }
    return;
  }

  // 4. Everything else is "info" text (ATV0 responses, etc.)
  if (this->listener_ != nullptr) {
    this->listener_->on_info_line(line);
  } else {
    ESP_LOGV(TAG, "Info line (no listener): '%s'", line.c_str());
  }
}

}  // namespace gme_xrs_radio
}  // namespace esphome
