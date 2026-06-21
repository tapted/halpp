/**
 * @file timer.hpp
 * @brief RAII wrapper for ESP-IDF LEDC (PWM) Timers
 */

#pragma once

#include <soc/gpio_num.h>
#include <hal/ledc_types.h>

#include "espbase/esp_result.hpp"

namespace HAL {

class Channel;

class Timer {
 public:
  constexpr Timer() = default;
  constexpr explicit Timer(ledc_mode_t mode, ledc_timer_t timer) : mode_(mode), timer_(timer) {}

  static EspResult<Timer> configure(ledc_timer_t timer_num, ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK,
                                    ledc_timer_bit_t resolution = LEDC_TIMER_13_BIT,
                                    uint32_t freq_hz = 4000,
                                    ledc_mode_t mode = LEDC_LOW_SPEED_MODE);

  EspResult<Channel> add_channel(ledc_channel_t channel, gpio_num_t gpio_num,
                                 uint32_t idle_level = 0);

  ~Timer() { reset(); }

  // Hardware handles cannot be copied, only moved.
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;
  Timer(Timer&& other) noexcept;
  Timer& operator=(Timer&& other) noexcept;

  explicit operator bool() const { return timer_ != LEDC_TIMER_MAX; }
  bool operator!() const { return timer_ == LEDC_TIMER_MAX; }

  EspResult<void> reset();
  EspResult<void> set_freq(uint32_t freq_hz);

 private:
  ledc_mode_t mode_ = LEDC_SPEED_MODE_MAX;
  ledc_timer_t timer_ = LEDC_TIMER_MAX;
};

}  // namespace HAL