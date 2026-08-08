#pragma once

#include "espbase/esp_result.hpp"
#include "halpp/audio/speaker.hpp"
#include "halpp/i2c/i2c_device.hpp"

namespace halpp {

enum class Es8311Resolution { Res16 = 16, Res18 = 18, Res20 = 20, Res24 = 24, Res32 = 32 };

enum class Es8311Fade {
  Off = 0,
  Fade4LRCK,  // 4LRCK means ramp 0.25dB/4LRCK
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
  // true: from MCLK pin (pin no. 2), false: from SCLK pin (pin no. 6)
  bool mclk_from_mclk_pin = true;
  uint32_t mclk_frequency = 44100 * 256;  //  ignored if MCLK is taken from SCLK pin
  uint32_t sample_frequency = 44100;      // Hz

  Es8311Resolution resolution = Es8311Resolution::Res16;
};

class Es8311 : public halpp::audio::Speaker {
 public:
  constexpr Es8311() = default;
  ~Es8311() { end_tx(); }

  EspResult<> set_voice_volume(int percent);
  EspResult<> get_voice_volume(int& percent) const;
  EspResult<> set_voice_mute(bool mute);

  EspResult<> set_voice_fade(Es8311Fade fade);
  EspResult<> set_microphone_fade(Es8311Fade fade);

  EspResult<> configure_microphone(bool digital_mic);
  EspResult<> configure_alc(bool enable);

 private:
  I2CDevice i2c_dev_;

  EspResult<> write_reg(uint8_t reg_addr, uint8_t data);
  EspResult<> read_reg(uint8_t reg_addr, uint8_t& data);

  EspResult<> config_clock(const Es8311Config& config);
  EspResult<> config_format(Es8311Resolution res);
  EspResult<> config_sample_frequency(uint32_t mclk, uint32_t sample_rate);

  // Takes ownership of a configured I2CDevice and initializes the codec
  EspResult<> start(I2CDevice i2c_dev, const Es8311Config& config);

  virtual EspResult<> on_set_hardware_volume(uint8_t percent) override;
  virtual EspResult<> init_default(uint8_t i2c_address) override;
  virtual EspResult<> reset() override;

  Es8311(const Es8311&) = delete;
  Es8311& operator=(const Es8311&) = delete;
};

}  // namespace halpp