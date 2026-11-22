#include "halpp/audio/microphone.hpp"

#include <cmath>
#include <driver/i2s_std.h>
#include <esp_log.h>

#include "halpp/audio/i2s_master.hpp"

constexpr const char TAG[] = "HALPP_MIC";

namespace halpp::audio {

uint32_t Microphone::calculate_rms(const int16_t* buffer, size_t sample_count) {
  if (!buffer || sample_count == 0) return 0;
  uint64_t sum_squares = 0;
  for (size_t i = 0; i < sample_count; ++i) {
    const int32_t s = buffer[i];
    sum_squares += static_cast<uint64_t>(s * s);
  }
  return static_cast<uint32_t>(std::sqrt(sum_squares / sample_count));
}

EspResult<> Microphone::start_rx() {
  if (rx_chan_) return ESP_OK;

  if (EspError err = I2SMaster::instance().retain_rx(&rx_chan_)) {
    return err.log(TAG, "retain_rx");
  }

  if (EspError err = init_default(halpp::config::Audio::MIC_I2C_ADDRESS)) {
    end_rx();
    return err.log(TAG, "init_default");
  }

  return ESP_OK;
}

EspResult<> Microphone::end_rx(bool for_powerdown) {
  EspResult<> ret = ESP_OK;

  if (for_powerdown) {
    ret = powerdown();
  } else {
    ret = reset();
  }

  if (rx_chan_) {
    I2SMaster::instance().release_rx();
    rx_chan_ = nullptr;
  }
  return ret;
}

EspResult<> Microphone::set_hardware_gain(float gain_db) {
  gain_db_ = gain_db;
  if (is_running()) {
    return on_set_hardware_gain(gain_db_);
  }
  return ESP_OK;
}

EspResult<> Microphone::set_hardware_volume(int8_t volume_db) {
  volume_db_ = volume_db;
  if (is_running()) {
    return on_set_hardware_volume(volume_db_);
  }
  return ESP_OK;
}

EspResult<size_t> Microphone::read(void* samples, size_t count, uint32_t timeout_ms) {
  if (!rx_chan_ || !samples || count == 0) return EspError(ESP_ERR_INVALID_STATE);

  size_t bytes_read = 0;

  // Read directly from the I2S DMA.
  esp_err_t err = i2s_channel_read(rx_chan_, samples, count, &bytes_read, timeout_ms);

  // Ignore timeout errors, as that just means the buffer is empty!
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) return EspError(err);
  return bytes_read;
}

}  // namespace halpp::audio