#include "halpp/audio/speaker.hpp"

#include <driver/i2s_std.h>
#include <esp_log.h>

#include "halpp/audio/i2s_master.hpp"

constexpr const char TAG[] = "HALPP_SPEAKER";

namespace halpp::audio {

EspResult<> Speaker::start_tx() {
  if (tx_chan_) return ESP_OK;

  if (EspError err = I2SMaster::instance().retain_tx(&tx_chan_)) {
    return err.log(TAG, "retain_tx");
  }

  if (EspError err = init_default(HAL::config::Audio::SPEAKER_I2C_ADDRESS)) {
    end_tx();
    return err.log(TAG, "init_default");
  }

  // Prep the GPIO for the Amp Enable pin.
  gpio_set_direction(config_.amp_enable_pin, GPIO_MODE_OUTPUT);  // log error?

  return set_amp_enable(true);
}

EspResult<> Speaker::end_tx() {
  // Mute everything before shutdown to prevent audio pops
  EspResult<> ret = set_amp_enable(false);

  // Shut down the codec and I2S
  reset();  // check error?

  if (tx_chan_) {
    I2SMaster::instance().release_tx();  // log error?
    tx_chan_ = nullptr;
  }
  return ret;
}

EspResult<> Speaker::set_amp_enable(bool enable) {
  // Amp enable pin is typically active high (1 = on, 0 = muted)
  return gpio_set_level(config_.amp_enable_pin, enable ? 1 : 0);
}

EspResult<> Speaker::set_hardware_volume(uint8_t percent) {
  volume_ = percent;
  if (is_running()) {
    return on_set_hardware_volume(volume_);
  }
  return ESP_OK;
}

}  // namespace halpp::audio