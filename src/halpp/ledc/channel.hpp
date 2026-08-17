/**
 * @file channel.hpp
 * @brief RAII wrapper for ESP-IDF LEDC (PWM) Channels
 */

#pragma once

#include <hal/ledc_types.h>

#include "espbase/esp_result.hpp"

namespace HAL {

class Channel {
 public:
  constexpr Channel() = default;

  constexpr explicit Channel(ledc_mode_t mode, ledc_channel_t channel, uint32_t idle_level = 0)
      : mode_(mode), channel_(channel), idle_level_(idle_level) {}
  ~Channel() { reset(); }

  // Movable, not copyable.
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
  Channel(Channel&& other) noexcept;
  Channel& operator=(Channel&& other) noexcept;

  explicit operator bool() const { return channel_ != LEDC_CHANNEL_MAX; }
  bool operator!() const { return channel_ == LEDC_CHANNEL_MAX; }

  // Temporarily halts the PWM output but retains ownership
  EspResult<> stop();

  // Halts output and permanently releases the handle
  EspResult<> reset();

  EspResult<> set_duty(uint32_t duty);
  EspResult<> update_duty();

  // Fading controls
  EspResult<> fade_stop();
  EspResult<> set_fade_with_time(uint32_t target_duty, int max_fade_time_ms);
  EspResult<> fade_start(ledc_fade_mode_t wait_done = LEDC_FADE_NO_WAIT);

 private:
  ledc_mode_t mode_ = LEDC_SPEED_MODE_MAX;
  ledc_channel_t channel_ = LEDC_CHANNEL_MAX;
  uint32_t idle_level_ = 0;
};

}  // namespace HAL