#pragma once

#include <cstddef>
#include <cstdint>

#include "espbase/esp_result.hpp"
#include "halpp/config.hpp"

struct i2s_channel_obj_t;
typedef struct i2s_channel_obj_t* i2s_chan_handle_t;

namespace halpp::audio {

class Microphone {
 public:
  virtual ~Microphone() = default;

  struct Config {
    uint32_t sample_rate = halpp::config::Audio::MIC_SAMPLE_RATE;
    i2s_data_bit_width_t bits_per_sample = halpp::config::Audio::MIC_BITS_PER_SAMPLE;
    float default_gain_db = halpp::config::Audio::MIC_DEFAULT_GAIN_DB;
    int8_t default_volume_db = halpp::config::Audio::MIC_DEFAULT_VOLUME_DB;
    bool tdm_enable = false;
  };

  const Config& config() const { return config_; }

  bool is_running() const { return rx_chan_ != nullptr; }
  i2s_chan_handle_t get_rx_handle() const { return rx_chan_; }

  // Retains the I2S RX channel and initializes the I2C ADC codec
  EspResult<> start_rx();

  // Resets or powers down the codec, then releases the I2S RX channel
  EspResult<> end_rx(bool powerdown = false);

  EspResult<> set_hardware_gain(float gain_db);
  float get_hardware_gain() const { return gain_db_; }

  EspResult<> set_hardware_volume(int8_t volume_db);
  int8_t get_hardware_volume() const { return volume_db_; }

  // Reads raw PCM bytes from the I2S RX DMA channel (timeout_ms matches ESP-IDF v5
  // i2s_channel_read)
  EspResult<size_t> read(void* samples, size_t count, uint32_t timeout_ms = 1000000);

  static uint32_t calculate_rms(const int16_t* buffer, size_t sample_count);

 protected:
  Config config_;
  float gain_db_ = 0.0f;
  int8_t volume_db_ = 0;
  i2s_chan_handle_t rx_chan_ = nullptr;

  constexpr Microphone() = default;
  explicit Microphone(Config config) : config_(config) {}

  virtual EspResult<> on_set_hardware_gain(float gain_db) = 0;
  virtual EspResult<> on_set_hardware_volume(int8_t volume_db) = 0;
  virtual EspResult<> init_default(uint8_t i2c_address) = 0;
  virtual EspResult<> reset() = 0;
  virtual EspResult<> powerdown() = 0;

  Microphone(const Microphone&) = delete;
  Microphone& operator=(const Microphone&) = delete;
  Microphone(Microphone&&) = delete;
  Microphone& operator=(Microphone&&) = delete;
};

}  // namespace halpp::audio