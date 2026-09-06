#pragma once

#include <cstdint>

#include "espbase/esp_result.hpp"
#include "halpp/ledc/channel.hpp"
#include "halpp/ledc/timer.hpp"

namespace halpp::display {

class Backlight {
 public:
  constexpr Backlight() = default;
  ~Backlight() = default;

  // Initializes the LEDC timer, channel, and fade service
  EspResult<> begin();
  EspResult<> reset();

  static uint32_t normalize_backlight_max_to_duty_max(uint8_t level, uint32_t duty_max);

  // Sets the backlight level (0 to config::Display::BACKLIGHT_MAX) with an optional fade
  EspResult<> set_level(uint8_t level, int fade_ms = 0);

  uint8_t get_level() const { return level_; }

 private:
  HAL::Timer timer_;
  HAL::Channel channel_;
  uint8_t level_ = 0;

  Backlight(const Backlight&) = delete;
  Backlight& operator=(const Backlight&) = delete;
  Backlight(Backlight&&) = delete;
  Backlight& operator=(Backlight&&) = delete;
};

}  // namespace halpp::display