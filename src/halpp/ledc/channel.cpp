#include "halpp/ledc/channel.hpp"

#include <driver/ledc.h>

namespace HAL {

Channel::Channel(Channel&& other) noexcept {
  mode_ = other.mode_;
  channel_ = other.channel_;
  idle_level_ = other.idle_level_;
  other.channel_ = LEDC_CHANNEL_MAX;
}

Channel& Channel::operator=(Channel&& other) noexcept {
  if (this != &other) {
    reset();
    mode_ = other.mode_;
    channel_ = other.channel_;
    idle_level_ = other.idle_level_;
    other.channel_ = LEDC_CHANNEL_MAX;
  }
  return *this;
}

EspResult<> Channel::stop() {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_stop(mode_, channel_, idle_level_);
}

EspResult<> Channel::reset() {
  if (channel_ != LEDC_CHANNEL_MAX) {
    ledc_channel_config_t deconfig{};
    deconfig.speed_mode = mode_;
    deconfig.channel = channel_;
    deconfig.deconfigure = true;
    esp_err_t err = ledc_channel_config(&deconfig);
    channel_ = LEDC_CHANNEL_MAX;
    return err;
  }
  return ESP_OK;
}

EspResult<> Channel::set_duty(uint32_t duty) {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_set_duty(mode_, channel_, duty);
}

EspResult<> Channel::update_duty() {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_update_duty(mode_, channel_);
}

EspResult<> Channel::fade_stop() {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_fade_stop(mode_, channel_);
}

EspResult<> Channel::set_fade_with_time(uint32_t target_duty, int max_fade_time_ms) {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_set_fade_with_time(mode_, channel_, target_duty, max_fade_time_ms);
}

EspResult<> Channel::fade_start(ledc_fade_mode_t wait_done) {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_fade_start(mode_, channel_, wait_done);
}

}  // namespace HAL