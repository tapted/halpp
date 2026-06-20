#include "halpp/ledc/channel.hpp"

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

EspResult<void> Channel::stop() {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_stop(mode_, channel_, idle_level_);
}

EspResult<void> Channel::reset() {
  if (channel_ != LEDC_CHANNEL_MAX) {
    esp_err_t err = ledc_stop(mode_, channel_, idle_level_);
    channel_ = LEDC_CHANNEL_MAX;
    return err;
  }
  return ESP_OK;
}

EspResult<void> Channel::set_duty(uint32_t duty) {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_set_duty(mode_, channel_, duty);
}

EspResult<void> Channel::update_duty() {
  if (!(*this)) return ESP_ERR_INVALID_STATE;
  return ledc_update_duty(mode_, channel_);
}

}  // namespace HAL