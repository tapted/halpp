#include "halpp/display/backlight.hpp"

#include <cmath>
#include <driver/ledc.h>
#include <esp_log.h>

#include "halpp/config.hpp"

static constexpr const char TAG[] = "BACKLIGHT";

namespace halpp::display {

static ledc_clk_cfg_t get_ledc_clk_cfg(config::Display::ClockSource clock_source) {
  switch (clock_source) {
    case config::Display::ClockSource::AUTO:
      return LEDC_AUTO_CLK;
    case config::Display::ClockSource::PLL:
      return LEDC_USE_PLL_DIV_CLK;
    case config::Display::ClockSource::RTC:
      return LEDC_USE_RC_FAST_CLK;
    case config::Display::ClockSource::XTAL:
      return LEDC_USE_XTAL_CLK;
  }
  return LEDC_AUTO_CLK;
}

EspResult<> Backlight::begin() {
  if (channel_) {
    return ESP_OK;  // Already initialized
  }
  ESP_LOGI(TAG, "Initializing backlight...");

  EspResult<HAL::Timer> timer_res =
      HAL::Timer::configure(config::Display::BACKLIGHT_LEDC_TIMER,                      //
                            get_ledc_clk_cfg(config::Display::BACKLIGHT_CLOCK_SOURCE),  //
                            config::Display::BACKLIGHT_LEDC_RESOLUTION,                 //
                            config::Display::BACKLIGHT_LEDC_FREQ,                       //
                            LEDC_LOW_SPEED_MODE);
  if (!timer_res) return timer_res.strip().log_error(TAG, "Failed to configure backlight timer");

  EspResult<HAL::Channel> chan_res = timer_res->add_channel(config::Display::BACKLIGHT_LEDC_CHANNEL,
                                                            config::Display::PIN_BACKLIGHT_PWM,
                                                            0);  // idle_level
  if (!chan_res) return chan_res.strip().log_error(TAG, "Failed to configure backlight channel");

  // Install the fade service globally. It returns ESP_ERR_INVALID_STATE if already installed by
  // another component, which is safe to ignore.
  esp_err_t fade_err = ledc_fade_func_install(0);
  if (fade_err != ESP_OK && fade_err != ESP_ERR_INVALID_STATE) {
    return EspError(fade_err).log(TAG, "Failed to install LEDC fade function");
  }

  timer_ = std::move(*timer_res);
  channel_ = std::move(*chan_res);

  ESP_LOGI(TAG, "Backlight initialized");
  return ESP_OK;
}

EspResult<> Backlight::reset() {
  ledc_fade_func_uninstall();
  channel_.reset();
  return timer_.reset();
}

EspResult<> Backlight::set_level(uint8_t level, int fade_ms) {
  if (level > config::Display::BACKLIGHT_MAX) {
    level = config::Display::BACKLIGHT_MAX;
  }

  ESP_LOGI(TAG, "Setting backlight to %d (fade_ms=%d)", level, fade_ms);
  level_ = level;

  // Calculate duty cycle
  uint32_t max_duty = (1 << config::Display::BACKLIGHT_LEDC_RESOLUTION) - 1;
  // Gamma 2.2 Perception Correction
  // Maps linear UI percentages to logarithmic human eye sensitivity
  float normalized = static_cast<float>(level) / static_cast<float>(config::Display::BACKLIGHT_MAX);
  float gamma_corrected = std::powf(normalized, 2.2f);
  uint32_t duty = static_cast<uint32_t>(gamma_corrected * static_cast<float>(max_duty) + 0.5f);

  // Interrupt any fades currently in progress
  if (EspError err = channel_.fade_stop()) {
    return err.log(TAG, "Failed to stop active fade");
  }

  if (fade_ms > 0) {
    if (EspError err = channel_.set_fade_with_time(duty, fade_ms)) return err;
    return channel_.fade_start(LEDC_FADE_NO_WAIT);
  } else {
    if (EspError err = channel_.set_duty(duty)) return err;
    return channel_.update_duty();
  }
}

}  // namespace halpp::display