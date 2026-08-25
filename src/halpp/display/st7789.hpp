#pragma once

#include <hal/spi_types.h>
#include <optional>

#include "halpp/display/backlight.hpp"
#include "halpp/display/display.hpp"

namespace halpp {

class St7789 : public Display {
 public:
  using Display::Display;

  static St7789& default_instance() {
    static std::optional<St7789> inst;
    if (!inst) inst.emplace();
    return *inst;
  }

  // Initializes the SPI IO and links it to the Display base class
  static EspResult<void> init_default_spi();
  static EspResult<void> deinit_default() { return default_instance().reset(); }

  // Initializes the ST7789 panel hardware
  EspResult<void> begin();

  void set_backlight(bool on, uint8_t brightness) {
    if (on) {
      backlight_.begin();
      backlight_.set_level(brightness);
    } else {
      backlight_.reset();
    }
  }

 private:
  halpp::display::Backlight backlight_;
};

}  // namespace halpp