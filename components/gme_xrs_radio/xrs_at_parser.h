#pragma once

#include <string>
#include <cstddef>
#include "esphome/core/defines.h"

namespace esphome {
namespace gme_xrs_radio {

/// Result code from the AT modem.
enum class ATResultCode {
  OK,
  ERROR,
};

/// Interface for receiving parsed AT events.
class ATParserListener {
 public:
  virtual ~ATParserListener() = default;

  /// Called when a final result code is seen ("OK" or "ERROR").
  virtual void on_result_code(ATResultCode code) = 0;

  /// Called when a line starting with '+' is seen.
  /// Example: "+WGPOW: 1" -> name="WGPOW", payload="1"
  virtual void on_plus_line(const std::string &name, const std::string &payload) = 0;

  /// Called for non-'+' informational lines (typically responses in ATV0 mode).
  /// Example: "GME" (from AT+GMI?)
  virtual void on_info_line(const std::string &line) = 0;

  /// Echo from the radio (AT commands echoed back when ATE1).
  /// Default no-op so you can ignore if you like.
  virtual void on_echo(const std::string &line) {}

  /// Fallback if something really doesn't fit any known pattern.
  /// Default no-op; you can override for extra logging.
  virtual void on_unknown_line(const std::string &line) {}
};

/// Simple line-based AT parser:
/// - Feed bytes with feed()/feed(const uint8_t*, size_t)
/// - Detects line ends on CR and/or LF
/// - Classifies:
///   * "OK"/"ERROR" -> result code
///   * "+XXX: payload" or "+XXX" -> plus line
///   * "AT..." -> echo
///   * everything else -> info line
class ATParser {
 public:
  explicit ATParser(ATParserListener *listener = nullptr);

  void set_listener(ATParserListener *listener) { this->listener_ = listener; }

  /// Reset internal line buffer.
  void reset();

  /// Feed a single character from the stream.
  void feed(char c);

  /// Feed a buffer of bytes from the stream.
  void feed(const uint8_t *data, size_t len);

 private:
  void handle_complete_line_(std::string &line);
  static void trim_whitespace_(std::string &s);

  std::string buffer_;
  ATParserListener *listener_{nullptr};

  static constexpr size_t MAX_LINE_LENGTH = 256;
};

}  // namespace gme_xrs_radio
}  // namespace esphome
