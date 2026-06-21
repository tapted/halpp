#include "halpp/ledc/timer.hpp"

#include <driver/ledc.h>

#include "halpp/ledc/channel.hpp"

namespace HAL {

EspResult<Timer> Timer::configure(ledc_timer_t timer_num, ledc_clk_cfg_t clk_cfg,
                                  ledc_timer_bit_t resolution, uint32_t freq_hz, ledc_mode_t mode) {
  ledc_timer_config_t timer_conf = {
      .speed_mode = mode,
      .duty_resolution = resolution,
      .timer_num = timer_num,
      .freq_hz = freq_hz,
      .clk_cfg = clk_cfg,
      .deconfigure = false,
  };

  if (esp_err_t err = ledc_timer_config(&timer_conf); err != ESP_OK) {
    return EspResult<Timer>::fail(err);
  }
  return EspResult<Timer>::ok(Timer(mode, timer_num));
}

EspResult<Channel> Timer::add_channel(ledc_channel_t channel, gpio_num_t gpio_num,
                                      uint32_t idle_level) {
  if (!(*this)) return ESP_ERR_INVALID_STATE;

  ledc_channel_config_t channel_config = {
      .gpio_num = gpio_num,
      .speed_mode = mode_,
      .channel = channel,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = timer_,  // Automatically bound to THIS timer
      .duty = 0,
      .hpoint = 0,
      .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
      .flags = {},
      .deconfigure = false,
  };

  if (esp_err_t err = ledc_channel_config(&channel_config); err != ESP_OK) {
    return EspResult<Channel>::fail(err);
  }

  // Returns the independent RAII wrapper upon success
  return EspResult<Channel>::ok(Channel(mode_, channel, idle_level));
}

Timer::Timer(Timer&& other) noexcept {
  mode_ = other.mode_;
  timer_ = other.timer_;
  other.timer_ = LEDC_TIMER_MAX;
}

Timer& Timer::operator=(Timer&& other) noexcept {
  if (this != &other) {
    reset();
    mode_ = other.mode_;
    timer_ = other.timer_;
    other.timer_ = LEDC_TIMER_MAX;
  }
  return *this;
}

EspResult<void> Timer::reset() {
  if (timer_ != LEDC_TIMER_MAX) {
    ledc_timer_pause(mode_, timer_);

    // Use ESP-IDF 5.x deconfigure flag to completely release the hardware timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = mode_,
        .duty_resolution = LEDC_TIMER_13_BIT,  // Ignored on deconfigure
        .timer_num = timer_,
        .freq_hz = 1000,           // Ignored
        .clk_cfg = LEDC_AUTO_CLK,  // Ignored
        .deconfigure = true,
    };
    esp_err_t err = ledc_timer_config(&timer_conf);
    timer_ = LEDC_TIMER_MAX;
    return err;
  }
  return ESP_OK;
}

EspResult<void> Timer::set_freq(uint32_t freq_hz) {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_set_freq(mode_, timer_, freq_hz);
}

}  // namespace HAL