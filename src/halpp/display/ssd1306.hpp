/**
 * @file ssd1306.hpp
 * @brief SSD1306 specific initialization, inheriting from the generic Display class.
 */

#pragma once

#include <optional>

#include "halpp/display/display.hpp"

namespace HAL {

class Ssd1306 : public Display {
 public:
  using Display::Display;  // Inherit constructors

  // --- Single-Device Default Pattern ---
  static Ssd1306& default_instance() {
    static std::optional<Ssd1306> inst;
    if (!inst) inst.emplace();
    return *inst;
  }

  // Factory convenience method
  static EspResult<void> init_default_i2c(uint8_t i2c_address = 0x3C, uint16_t width = 128,
                                          uint16_t height = 64);

  static EspResult<void> deinit_default() { return default_instance().reset(); }

  // Initializes the OLED chip hardware
  EspResult<void> begin();
};

}  // namespace HAL