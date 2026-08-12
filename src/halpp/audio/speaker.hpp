#pragma once

#include "espbase/esp_result.hpp"
#include "halpp/config.hpp"

struct i2s_channel_obj_t;
typedef struct i2s_channel_obj_t* i2s_chan_handle_t;

namespace halpp::audio {

class Speaker {
 public:
  virtual ~Speaker() = default;

  struct Config {
    uint32_t sample_rate = HAL::config::Audio::SAMPLE_RATE;
    i2s_data_bit_width_t bits_per_sample = HAL::config::Audio::BITS_PER_SAMPLE;
    uint8_t default_volume = HAL::config::Audio::DEFAULT_VOLUME;
    gpio_num_t amp_enable_pin = HAL::config::Audio::PIN_AMP_ENABLE;
    bool is_stereo = HAL::config::Audio::IS_STEREO;
  };

  const Config& config() const { return config_; }

  bool is_running() const { return tx_chan_ != nullptr; }
  i2s_chan_handle_t get_tx_handle() const { return tx_chan_; }

  // Initializes the I2C codec, I2S DMA, and powers on the Amp
  EspResult<> start_tx();

  // Safely powers down the Amp and releases hardware buses
  EspResult<> end_tx();

  EspResult<> set_amp_enable(bool enable);
  EspResult<> set_hardware_volume(uint8_t percent);
  uint8_t get_hardware_volume() const { return volume_; }

  EspResult<size_t> write(const void* samples, size_t count, uint32_t timeout_ms = 1000000);

 protected:
  Config config_;
  uint8_t volume_ = 0;
  i2s_chan_handle_t tx_chan_ = nullptr;

  constexpr Speaker() = default;
  explicit Speaker(Config config) : config_(config) {}

  virtual EspResult<> on_set_hardware_volume(uint8_t percent) = 0;
  virtual EspResult<> init_default(uint8_t i2c_address) = 0;
  virtual EspResult<> reset() = 0;

  Speaker(const Speaker&) = delete;
  Speaker& operator=(const Speaker&) = delete;
  Speaker(Speaker&&) = delete;
  Speaker& operator=(Speaker&&) = delete;
};

}  // namespace halpp::audio