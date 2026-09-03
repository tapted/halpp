#pragma once

#include <hal/spi_types.h>
#include <optional>

#include "halpp/display/backlight.hpp"
#include "halpp/display/display.hpp"

namespace halpp {

enum class BacklightState {
  Off = 0,
  On = 1,
};

class SpiDisplay : public Display {
 public:
  using Display::Display;

  static SpiDisplay& default_instance() {
    static std::optional<SpiDisplay> inst;
    if (!inst) inst.emplace();
    return *inst;
  }

  // Initializes the SPI IO and links it to the Display base class
  static EspResult<void> init_default_spi();
  static EspResult<void> deinit_default() { return default_instance().reset(); }

  // Initializes the Display panel hardware
  EspResult<void> begin();

  uint8_t get_backlight() const { return backlight_.get_level(); }
  EspResult<void> set_backlight(BacklightState state, uint8_t brightness, int fade_ms = 500) {
    if (state == BacklightState::On) {
      if (EspError err = backlight_.begin()) return err;
      return backlight_.set_level(brightness, fade_ms);
    } else {
      return backlight_.reset();
    }
  }

 private:
  halpp::display::Backlight backlight_;
};

}  // namespace halpp