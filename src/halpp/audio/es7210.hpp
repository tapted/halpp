#pragma once

#include <cstdint>

#include "espbase/esp_result.hpp"
#include "halpp/audio/microphone.hpp"
#include "halpp/i2c/i2c_device.hpp"

namespace halpp {

enum class Es7210Resolution : uint8_t {
  Res16 = 16,
  Res18 = 18,
  Res20 = 20,
  Res24 = 24,
  Res32 = 32,
};

enum class Es7210I2sFormat : uint8_t {
  I2s = 0x00,
  LeftJustified = 0x01,
  DspA = 0x03,
  DspB = 0x13,
};

enum class Es7210MicGain : uint8_t {
  Gain0dB = 0,
  Gain3dB = 1,
  Gain6dB = 2,
  Gain9dB = 3,
  Gain12dB = 4,
  Gain15dB = 5,
  Gain18dB = 6,
  Gain21dB = 7,
  Gain24dB = 8,
  Gain27dB = 9,
  Gain30dB = 10,
  Gain33dB = 11,
  Gain34_5dB = 12,
  Gain36dB = 13,
  Gain37_5dB = 14,
};

enum class Es7210MicBias : uint8_t {
  Bias2V18 = 0x00,
  Bias2V26 = 0x10,
  Bias2V36 = 0x20,
  Bias2V45 = 0x30,
  Bias2V55 = 0x40,
  Bias2V66 = 0x50,
  Bias2V78 = 0x60,
  Bias2V87 = 0x70,
};

struct Es7210Config {
  uint32_t sample_rate_hz = 16000;
  uint32_t mclk_ratio = 256;
  Es7210I2sFormat i2s_format = Es7210I2sFormat::I2s;
  Es7210Resolution resolution = Es7210Resolution::Res16;
  Es7210MicBias mic_bias = Es7210MicBias::Bias2V87;
  Es7210MicGain mic_gain = Es7210MicGain::Gain24dB;
  bool tdm_enable = false;
};

class Es7210 : public halpp::audio::Microphone {
 public:
  static constexpr uint8_t ADDRESS_00 = 0x40;
  static constexpr uint8_t ADDRESS_01 = 0x41;
  static constexpr uint8_t ADDRESS_10 = 0x42;
  static constexpr uint8_t ADDRESS_11 = 0x43;

  constexpr Es7210() = default;
  ~Es7210() { end_rx(true /* powerdown */); }

  EspResult<> set_mic_gain(Es7210MicGain gain);
  EspResult<> set_mic_bias(Es7210MicBias bias);
  EspResult<> set_digital_volume(int8_t volume_db);  // [-95, +32] dB

  EspResult<> set_channel_gain(uint8_t channel, Es7210MicGain gain);  // channel: 1..4
  EspResult<> set_channel_volume(uint8_t channel, int8_t volume_db);  // channel: 1..4
  EspResult<> set_channel_enabled(uint8_t channel, bool enable);      // channel: 1..4

  EspResult<> set_mic_mute(bool mute);
  EspResult<> configure_alc(bool enable, bool enable_automute);

 private:
  I2CDevice i2c_dev_;

  EspResult<> write_reg(uint8_t reg_addr, uint8_t data);
  EspResult<> read_reg(uint8_t reg_addr, uint8_t& data);

  EspResult<> config_format(Es7210I2sFormat format, Es7210Resolution res, bool tdm_enable);
  EspResult<> config_sample_rate(uint32_t sample_rate_hz, uint32_t mclk_ratio);

  // Takes ownership of a configured I2CDevice and initializes the ADC
  EspResult<> start(I2CDevice i2c_dev, const Es7210Config& config);

  virtual EspResult<> on_set_hardware_gain(float gain_db) override;
  virtual EspResult<> on_set_hardware_volume(int8_t volume_db) override;
  virtual EspResult<> init_default(uint8_t i2c_address) override;
  virtual EspResult<> reset() override;
  virtual EspResult<> powerdown() override;

  Es7210(const Es7210&) = delete;
  Es7210& operator=(const Es7210&) = delete;
};

}  // namespace halpp