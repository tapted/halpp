#pragma once

#include "halpp/display/backlight.hpp"
#include "halpp/display/display.hpp"

namespace halpp {

class GenericDisplay : public Display {
 public:
  using Display::Display;

  // Initializes the Display panel hardware
  EspResult<> begin() override;

  uint8_t get_backlight() const override { return backlight_.get_level(); }
  EspResult<> set_backlight(BacklightState state, uint8_t brightness, int fade_ms = 500) override {
    if (state == BacklightState::On) {
      if (EspError err = backlight_.begin()) return err;
      return backlight_.set_level(brightness, fade_ms);
    } else {
      return backlight_.reset();
    }
  }

 private:
  display::Backlight backlight_;
};

}  // namespace halpp