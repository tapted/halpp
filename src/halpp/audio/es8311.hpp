#pragma once

#include "espbase/esp_result.hpp"
#include "halpp/i2c/i2c_device.hpp"

namespace halpp {

enum class Es8311Resolution { Res16 = 16, Res18 = 18, Res20 = 20, Res24 = 24, Res32 = 32 };

enum class Es8311Fade {
  Off = 0,
  Fade4LRCK,
  Fade8LRCK,
  Fade16LRCK,
  Fade32LRCK,
  Fade64LRCK,
  Fade128LRCK,
  Fade256LRCK,
  Fade512LRCK,
  Fade1024LRCK,
  Fade2048LRCK,
  Fade4096LRCK,
  Fade8192LRCK,
  Fade16384LRCK,
  Fade32768LRCK,
  Fade65536LRCK
};

struct Es8311Config {
  bool mclk_inverted = false;
  bool sclk_inverted = false;
  bool mclk_from_mclk_pin = true;
  uint32_t mclk_frequency = 44100 * 256;
  uint32_t sample_frequency = 44100;

  Es8311Resolution resolution = Es8311Resolution::Res16;
};
class Es8311 {
 public:
  constexpr Es8311() = default;
  ~Es8311() = default;

  // Prevent copying to maintain strict ownership of the I2C device
  Es8311(const Es8311&) = delete;
  Es8311& operator=(const Es8311&) = delete;

  // Takes ownership of a configured I2CDevice and initializes the codec
  EspResult<void> start(I2CDevice i2c_dev, const Es8311Config& config);
  void reset();

  EspResult<void> set_voice_volume(int percent);
  EspResult<void> get_voice_volume(int& percent) const;
  EspResult<void> set_voice_mute(bool mute);

  EspResult<void> set_voice_fade(Es8311Fade fade);
  EspResult<void> set_microphone_fade(Es8311Fade fade);

  EspResult<void> configure_microphone(bool digital_mic);
  EspResult<void> configure_alc(bool enable);

 private:
  I2CDevice i2c_dev_;

  EspResult<void> write_reg(uint8_t reg_addr, uint8_t data);
  EspResult<void> read_reg(uint8_t reg_addr, uint8_t& data);

  EspResult<void> config_clock(const Es8311Config& config);
  EspResult<void> config_format(Es8311Resolution res);
  EspResult<void> config_sample_frequency(uint32_t mclk, uint32_t sample_rate);
};

}  // namespace halpp