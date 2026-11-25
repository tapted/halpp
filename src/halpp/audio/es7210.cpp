#include "halpp/audio/es7210.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "halpp/i2c/i2c_master.hpp"

namespace halpp {

namespace {

constexpr const char TAG[] = "Es7210";

// Register map
constexpr uint8_t ES7210_RESET_REG00 = 0x00;
constexpr uint8_t ES7210_CLOCK_OFF_REG01 = 0x01;
constexpr uint8_t ES7210_MAINCLK_REG02 = 0x02;
constexpr uint8_t ES7210_MASTER_CLK_REG03 = 0x03;
constexpr uint8_t ES7210_LRCK_DIVH_REG04 = 0x04;
constexpr uint8_t ES7210_LRCK_DIVL_REG05 = 0x05;
constexpr uint8_t ES7210_POWER_DOWN_REG06 = 0x06;
constexpr uint8_t ES7210_OSR_REG07 = 0x07;
constexpr uint8_t ES7210_MODE_CONFIG_REG08 = 0x08;
constexpr uint8_t ES7210_TIME_CONTROL0_REG09 = 0x09;
constexpr uint8_t ES7210_TIME_CONTROL1_REG0A = 0x0A;
constexpr uint8_t ES7210_SDP_INTERFACE1_REG11 = 0x11;
constexpr uint8_t ES7210_SDP_INTERFACE2_REG12 = 0x12;
constexpr uint8_t ES7210_ADC34_MUTERANGE_REG14 = 0x14;
constexpr uint8_t ES7210_ADC1_DIRECT_DB_REG1B = 0x1B;
constexpr uint8_t ES7210_ADC2_DIRECT_DB_REG1C = 0x1C;
constexpr uint8_t ES7210_ADC3_DIRECT_DB_REG1D = 0x1D;
constexpr uint8_t ES7210_ADC4_DIRECT_DB_REG1E = 0x1E;
constexpr uint8_t ES7210_ADC34_HPF2_REG20 = 0x20;
constexpr uint8_t ES7210_ADC34_HPF1_REG21 = 0x21;
constexpr uint8_t ES7210_ADC12_HPF2_REG22 = 0x22;
constexpr uint8_t ES7210_ADC12_HPF1_REG23 = 0x23;
constexpr uint8_t ES7210_ANALOG_REG40 = 0x40;
constexpr uint8_t ES7210_MIC12_BIAS_REG41 = 0x41;
constexpr uint8_t ES7210_MIC34_BIAS_REG42 = 0x42;
constexpr uint8_t ES7210_MIC1_GAIN_REG43 = 0x43;
constexpr uint8_t ES7210_MIC2_GAIN_REG44 = 0x44;
constexpr uint8_t ES7210_MIC3_GAIN_REG45 = 0x45;
constexpr uint8_t ES7210_MIC4_GAIN_REG46 = 0x46;
constexpr uint8_t ES7210_MIC1_POWER_REG47 = 0x47;
constexpr uint8_t ES7210_MIC2_POWER_REG48 = 0x48;
constexpr uint8_t ES7210_MIC3_POWER_REG49 = 0x49;
constexpr uint8_t ES7210_MIC4_POWER_REG4A = 0x4A;
constexpr uint8_t ES7210_MIC12_POWER_REG4B = 0x4B;
constexpr uint8_t ES7210_MIC34_POWER_REG4C = 0x4C;

// Automute & ALC Registers (ES7210 Datasheet Rev 2.0, Regs 0x13 - 0x1E)
constexpr uint8_t ES7210_ADC_AUTOMUTE_REG13 = 0x13;  // Automute enable & mute ramp control
constexpr uint8_t ES7210_ADC1_DIRECT_DB_REG1E = 0x1E;
constexpr uint8_t ES7210_ADC34_MUTE_REG14 = 0x14;     // ADC3/4 automute detection range
constexpr uint8_t ES7210_ADC12_MUTE_REG15 = 0x15;     // ADC1/2 automute detection range
constexpr uint8_t ES7210_ALC_SEL_REG16 = 0x16;        // ALC channel pair enable
constexpr uint8_t ES7210_ALC_COM_CFG1_REG17 = 0x17;   // ALC attack, decay, hold & zero-cross
constexpr uint8_t ES7210_ALC34_LVL_REG18 = 0x18;      // ADC3/4 ALC max/min target window
constexpr uint8_t ES7210_ALC12_LVL_REG19 = 0x19;      // ADC1/2 ALC max/min target window
constexpr uint8_t ES7210_ALC_COM_CFG2_REG1A = 0x1A;   // ALC window size & noise-gate link
constexpr uint8_t ES7210_ADC4_MAX_GAIN_REG1B = 0x1B;  // ADC4 direct dB / ALC max gain
constexpr uint8_t ES7210_ADC3_MAX_GAIN_REG1C = 0x1C;  // ADC3 direct dB / ALC max gain
constexpr uint8_t ES7210_ADC2_MAX_GAIN_REG1D = 0x1D;  // ADC2 direct dB / ALC max gain
constexpr uint8_t ES7210_ADC1_MAX_GAIN_REG1E = 0x1E;  // ADC1 direct dB / ALC max gain

// ALC & Automute Bitfields
constexpr uint8_t ALC16_ADC12_OFF = 0x00;
constexpr uint8_t ALC16_ADC12_STEREO = 0x03;  // Bits [1:0] = 11 (ADC1/2 stereo ALC)
constexpr uint8_t ALC16_ADC34_STEREO = 0x0C;  // Bits [3:2] = 11 (ADC3/4 stereo ALC)

constexpr uint8_t AUTOMUTE13_DISABLE = 0x00;
constexpr uint8_t AUTOMUTE13_ADC12_EN = 0x03;      // Enable automute on ADC1 & ADC2
constexpr uint8_t AUTOMUTE15_VOICE_THRESH = 0x14;  // Moderate noise-gate threshold

constexpr uint8_t ALC17_VOICE_TIMING = 0x34;   // Fast attack, moderate decay, ZC on
constexpr uint8_t ALC19_TARGET_WINDOW = 0xC9;  // Max -7.8 dBFS (0xC), Min -10.1 dBFS (0x9)
constexpr uint8_t ALC1A_VOICE_CFG2 = 0x14;     // Moderate hold / window config

constexpr uint8_t ADC_GAIN_0DB = 0xBF;        // 0.0 dB (0.5 dB/step; 0x00 = -95.5 dB)
constexpr uint8_t ADC_GAIN_PLUS_16DB = 0xDF;  // +16.0 dB ALC digital boost ceiling

struct CoeffDiv {
  uint32_t mclk;
  uint32_t lrck;
  uint8_t ss_ds;
  uint8_t adc_div;
  uint8_t dll;
  uint8_t doubler;
  uint8_t osr;
  uint8_t mclk_src;
  uint8_t lrck_h;
  uint8_t lrck_l;
};

static constexpr CoeffDiv coeff_table[] = {
    // mclk      lrck   ss_ds adc_div dll  doubler osr  mclk_src lrckh lrckl
    /* 8k */
    {12288000, 8000, 0x00, 0x03, 0x01, 0x00, 0x20, 0x00, 0x06, 0x00},
    {16384000, 8000, 0x00, 0x04, 0x01, 0x00, 0x20, 0x00, 0x08, 0x00},
    {19200000, 8000, 0x00, 0x1e, 0x00, 0x01, 0x28, 0x00, 0x09, 0x60},
    {4096000, 8000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},

    /* 11.025k */
    {11289600, 11025, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x01, 0x00},

    /* 12k */
    {12288000, 12000, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x04, 0x00},
    {19200000, 12000, 0x00, 0x14, 0x00, 0x01, 0x28, 0x00, 0x06, 0x40},

    /* 16k */
    {4096000, 16000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {19200000, 16000, 0x00, 0x0a, 0x00, 0x00, 0x1e, 0x00, 0x04, 0x80},
    {16384000, 16000, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x04, 0x00},
    {12288000, 16000, 0x00, 0x03, 0x01, 0x01, 0x20, 0x00, 0x03, 0x00},

    /* 22.05k */
    {11289600, 22050, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},

    /* 24k */
    {12288000, 24000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},
    {19200000, 24000, 0x00, 0x0a, 0x00, 0x01, 0x28, 0x00, 0x03, 0x20},

    /* 32k */
    {12288000, 32000, 0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x01, 0x80},
    {16384000, 32000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},
    {19200000, 32000, 0x00, 0x05, 0x00, 0x00, 0x1e, 0x00, 0x02, 0x58},

    /* 44.1k */
    {11289600, 44100, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},

    /* 48k */
    {12288000, 48000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {19200000, 48000, 0x00, 0x05, 0x00, 0x01, 0x28, 0x00, 0x01, 0x90},

    /* 64k */
    {16384000, 64000, 0x01, 0x01, 0x01, 0x00, 0x20, 0x00, 0x01, 0x00},
    {19200000, 64000, 0x00, 0x05, 0x00, 0x01, 0x1e, 0x00, 0x01, 0x2c},

    /* 88.2k */
    {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x80},

    /* 96k */
    {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x80},
    {19200000, 96000, 0x01, 0x05, 0x00, 0x01, 0x28, 0x00, 0x00, 0xc8},
};

const CoeffDiv* get_coeff(uint32_t mclk, uint32_t lrck) {
  for (const auto& coeff : coeff_table) {
    if (coeff.lrck == lrck && coeff.mclk == mclk) {
      return &coeff;
    }
  }
  return nullptr;
}

Es7210MicGain gain_from_db(float gain_db) {
  if (gain_db <= 0.0f) return Es7210MicGain::Gain0dB;
  if (gain_db >= 37.5f) return Es7210MicGain::Gain37_5dB;
  if (gain_db >= 36.0f) return Es7210MicGain::Gain36dB;
  if (gain_db >= 34.5f) return Es7210MicGain::Gain34_5dB;
  // 0 dB .. 33 dB are exact 3 dB steps (enum values 0 .. 11)
  uint8_t step = static_cast<uint8_t>((gain_db + 1.5f) / 3.0f);
  if (step > 11) step = 11;
  return static_cast<Es7210MicGain>(step);
}

}  // namespace

EspResult<> Es7210::reset() {
  if (i2c_dev_) {
    set_mic_mute(true);
  }
  return i2c_dev_.reset();
}

EspResult<> Es7210::powerdown() {
  if (!i2c_dev_) return ESP_ERR_INVALID_STATE;

  // 1. Mute digital outputs and drop digital ADC gain to minimum (-95.5 dB)
  if (EspError err = set_mic_mute(true)) return err;
  if (EspError err = set_digital_volume(-95)) return err;

  // 2. Power down MIC1-4 bias, PGA, and ADC blocks
  if (EspError err = write_reg(ES7210_MIC12_POWER_REG4B, 0x00)) return err;
  if (EspError err = write_reg(ES7210_MIC34_POWER_REG4C, 0x00)) return err;
  if (EspError err = write_reg(ES7210_MIC1_POWER_REG47, 0x00)) return err;
  if (EspError err = write_reg(ES7210_MIC2_POWER_REG48, 0x00)) return err;
  if (EspError err = write_reg(ES7210_MIC3_POWER_REG49, 0x00)) return err;
  if (EspError err = write_reg(ES7210_MIC4_POWER_REG4A, 0x00)) return err;

  // 3. Power down analog reference, bias generator, DLL, and VMID
  if (EspError err = write_reg(ES7210_POWER_DOWN_REG06, 0xFF)) return err;
  if (EspError err = write_reg(ES7210_ANALOG_REG40, 0x00)) return err;

  // 4. Turn off internal ADC clocks and halt the Chip State Machine
  if (EspError err = write_reg(ES7210_CLOCK_OFF_REG01, 0x7F)) return err;
  if (EspError err = write_reg(ES7210_RESET_REG00, 0x00)) return err;

  return i2c_dev_.reset();
}

EspResult<> Es7210::write_reg(uint8_t reg_addr, uint8_t data) {
  return i2c_dev_.write_reg(reg_addr, data);
}

EspResult<> Es7210::read_reg(uint8_t reg_addr, uint8_t& data) {
  uint8_t buf[1];
  if (EspError err = i2c_dev_.read_reg(reg_addr, buf)) return err;
  data = buf[0];
  return ESP_OK;
}

EspResult<> Es7210::start(I2CDevice i2c_dev, const Es7210Config& config) {
  if (!i2c_dev) return ESP_ERR_INVALID_ARG;
  if (i2c_dev_) return ESP_ERR_INVALID_STATE;

  i2c_dev_ = std::move(i2c_dev);

  // 1. Software reset (restores all registers cleanly even if woken from powerdown)
  if (EspError err = write_reg(ES7210_RESET_REG00, 0xFF)) return err;
  vTaskDelay(pdMS_TO_TICKS(10));
  if (EspError err = write_reg(ES7210_RESET_REG00, 0x32)) return err;

  // 2. Initialization and power-up timing
  if (EspError err = write_reg(ES7210_TIME_CONTROL0_REG09, 0x30)) return err;
  if (EspError err = write_reg(ES7210_TIME_CONTROL1_REG0A, 0x30)) return err;

  // 3. Configure HPF for ADC1-4
  if (EspError err = write_reg(ES7210_ADC12_HPF1_REG23, 0x2A)) return err;
  if (EspError err = write_reg(ES7210_ADC12_HPF2_REG22, 0x0A)) return err;
  if (EspError err = write_reg(ES7210_ADC34_HPF1_REG21, 0x2A)) return err;
  if (EspError err = write_reg(ES7210_ADC34_HPF2_REG20, 0x0A)) return err;

  // 4. Serial digital port format & sample rate
  if (EspError err = config_format(config.i2s_format, config.resolution, config.tdm_enable)) {
    return err;
  }

  // 5. Analog power, VMID, MIC bias & PGA gain
  if (EspError err = write_reg(ES7210_ANALOG_REG40, 0xC3)) return err;
  if (EspError err = set_mic_bias(config.mic_bias)) return err;
  if (EspError err = set_mic_gain(config.mic_gain)) return err;

  // 6. Power on MIC1-4 frontends and configure clocks
  if (EspError err = write_reg(ES7210_MIC1_POWER_REG47, 0x08)) return err;
  if (EspError err = write_reg(ES7210_MIC2_POWER_REG48, 0x08)) return err;
  if (EspError err = write_reg(ES7210_MIC3_POWER_REG49, 0x08)) return err;
  if (EspError err = write_reg(ES7210_MIC4_POWER_REG4A, 0x08)) return err;

  if (EspError err = config_sample_rate(config.sample_rate_hz, config.mclk_ratio)) return err;

  // 7. Power down DLL (bypassed), enable MIC bias + ADC1-4 + PGA1-4, and start CSM
  if (EspError err = write_reg(ES7210_POWER_DOWN_REG06, 0x04)) return err;
  if (EspError err = write_reg(ES7210_MIC12_POWER_REG4B, 0x0F)) return err;
  if (EspError err = write_reg(ES7210_MIC34_POWER_REG4C, 0x0F)) return err;
  if (EspError err = write_reg(ES7210_RESET_REG00, 0x71)) return err;
  if (EspError err = write_reg(ES7210_RESET_REG00, 0x41)) return err;

  return ESP_OK;
}

EspResult<> Es7210::config_format(Es7210I2sFormat format, Es7210Resolution res, bool tdm_enable) {
  uint8_t res_bits = 0;
  switch (res) {
    case Es7210Resolution::Res16:
      res_bits = 0x60;
      break;
    case Es7210Resolution::Res18:
      res_bits = 0x40;
      break;
    case Es7210Resolution::Res20:
      res_bits = 0x20;
      break;
    case Es7210Resolution::Res24:
      res_bits = 0x00;
      break;
    case Es7210Resolution::Res32:
      res_bits = 0x80;
      break;
  }

  if (EspError err =
          write_reg(ES7210_SDP_INTERFACE1_REG11, static_cast<uint8_t>(format) | res_bits)) {
    return err;
  }

  uint8_t tdm_val = 0x00;
  if (tdm_enable) {
    tdm_val =
        (format == Es7210I2sFormat::I2s || format == Es7210I2sFormat::LeftJustified) ? 0x02 : 0x01;
  }
  return write_reg(ES7210_SDP_INTERFACE2_REG12, tdm_val);
}

EspResult<> Es7210::config_sample_rate(uint32_t sample_rate_hz, uint32_t mclk_ratio) {
  const uint32_t mclk_freq_hz = sample_rate_hz * mclk_ratio;
  const CoeffDiv* coeff = get_coeff(mclk_freq_hz, sample_rate_hz);
  if (!coeff) {
    ESP_LOGE(TAG, "Unable to set %luHz sample rate with %luHz MCLK", sample_rate_hz, mclk_freq_hz);
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (EspError err = write_reg(ES7210_OSR_REG07, coeff->osr)) return err;
  if (EspError err = write_reg(ES7210_MAINCLK_REG02,
                               coeff->adc_div | (coeff->doubler << 6) | (coeff->dll << 7))) {
    return err;
  }
  if (EspError err = write_reg(ES7210_LRCK_DIVH_REG04, coeff->lrck_h)) return err;
  return write_reg(ES7210_LRCK_DIVL_REG05, coeff->lrck_l);
}

EspResult<> Es7210::set_mic_gain(Es7210MicGain gain) {
  const uint8_t reg_val = static_cast<uint8_t>(gain) | 0x10;
  if (EspError err = write_reg(ES7210_MIC1_GAIN_REG43, reg_val)) return err;
  if (EspError err = write_reg(ES7210_MIC2_GAIN_REG44, reg_val)) return err;
  if (EspError err = write_reg(ES7210_MIC3_GAIN_REG45, reg_val)) return err;
  return write_reg(ES7210_MIC4_GAIN_REG46, reg_val);
}

EspResult<> Es7210::set_mic_bias(Es7210MicBias bias) {
  const uint8_t reg_val = static_cast<uint8_t>(bias);
  if (EspError err = write_reg(ES7210_MIC12_BIAS_REG41, reg_val)) return err;
  return write_reg(ES7210_MIC34_BIAS_REG42, reg_val);
}

EspResult<> Es7210::set_digital_volume(int8_t volume_db) {
  if (volume_db < -95) volume_db = -95;
  if (volume_db > 32) volume_db = 32;

  // 0x00 = -95.5 dB, 0xBF (191) = 0 dB, 0xFF = +32 dB (0.5 dB/step)
  const uint8_t reg_val = static_cast<uint8_t>(191 + volume_db * 2);
  if (EspError err = write_reg(ES7210_ADC1_DIRECT_DB_REG1B, reg_val)) return err;
  if (EspError err = write_reg(ES7210_ADC2_DIRECT_DB_REG1C, reg_val)) return err;
  if (EspError err = write_reg(ES7210_ADC3_DIRECT_DB_REG1D, reg_val)) return err;
  return write_reg(ES7210_ADC4_DIRECT_DB_REG1E, reg_val);
}

EspResult<> Es7210::set_channel_gain(uint8_t channel, Es7210MicGain gain) {
  if (channel < 1 || channel > 4) return ESP_ERR_INVALID_ARG;
  const uint8_t reg = static_cast<uint8_t>(ES7210_MIC1_GAIN_REG43 + (channel - 1));
  return write_reg(reg, static_cast<uint8_t>(gain) | 0x10);
}

EspResult<> Es7210::set_channel_volume(uint8_t channel, int8_t volume_db) {
  if (channel < 1 || channel > 4) return ESP_ERR_INVALID_ARG;
  if (volume_db < -95) volume_db = -95;
  if (volume_db > 32) volume_db = 32;

  // Note: Digital gain registers run descending (0x1E = ADC1 .. 0x1B = ADC4)
  const uint8_t reg = static_cast<uint8_t>(ES7210_ADC1_DIRECT_DB_REG1E - (channel - 1));
  const uint8_t reg_val = static_cast<uint8_t>(191 + volume_db * 2);
  return write_reg(reg, reg_val);
}

EspResult<> Es7210::set_channel_enabled(uint8_t channel, bool enable) {
  if (channel < 1 || channel > 4) return ESP_ERR_INVALID_ARG;
  const uint8_t reg = static_cast<uint8_t>(ES7210_MIC1_POWER_REG47 + (channel - 1));
  return write_reg(reg, enable ? 0x08 : 0x00);
}

EspResult<> Es7210::set_mic_mute(bool mute) {
  uint8_t reg13 = 0;
  if (EspError err = read_reg(ES7210_ADC_AUTOMUTE_REG13, reg13)) return err;
  if (mute) {
    reg13 |= 0x03;  // Mute ADC1/2 and ADC3/4
  } else {
    reg13 &= ~0x03;
  }
  return write_reg(ES7210_ADC_AUTOMUTE_REG13, reg13);
}

EspResult<> Es7210::configure_alc(bool enable, bool enable_automute) {
  if (!i2c_dev_) return ESP_ERR_INVALID_STATE;

  if (!enable) {
    if (EspError err = write_reg(ES7210_ALC_SEL_REG16, ALC16_ADC12_OFF)) return err;
    if (EspError err = write_reg(ES7210_ADC_AUTOMUTE_REG13, AUTOMUTE13_DISABLE)) return err;
    if (EspError err = write_reg(ES7210_ADC1_MAX_GAIN_REG1E, ADC_GAIN_0DB)) return err;
    if (EspError err = write_reg(ES7210_ADC2_MAX_GAIN_REG1D, ADC_GAIN_0DB)) return err;
    return ESP_OK;
  }

  // 1. Set the ALC digital gain ceiling on MIC1 & MIC2 (+16 dB above analog PGA gain)
  if (EspError err = write_reg(ES7210_ADC1_MAX_GAIN_REG1E, ADC_GAIN_PLUS_16DB)) return err;
  if (EspError err = write_reg(ES7210_ADC2_MAX_GAIN_REG1D, ADC_GAIN_PLUS_16DB)) return err;

  // 2. Configure target level window (-7.8 dBFS to -10.1 dBFS) and attack/decay timing
  if (EspError err = write_reg(ES7210_ALC12_LVL_REG19, ALC19_TARGET_WINDOW)) return err;
  if (EspError err = write_reg(ES7210_ALC_COM_CFG1_REG17, ALC17_VOICE_TIMING)) return err;
  if (EspError err = write_reg(ES7210_ALC_COM_CFG2_REG1A, ALC1A_VOICE_CFG2)) return err;

  // 3. Configure hardware automute (noise gate) on ADC1 & ADC2
  if (enable_automute) {
    if (EspError err = write_reg(ES7210_ADC12_MUTE_REG15, AUTOMUTE15_VOICE_THRESH)) return err;
    if (EspError err = write_reg(ES7210_ADC_AUTOMUTE_REG13, AUTOMUTE13_ADC12_EN)) return err;
  } else {
    if (EspError err = write_reg(ES7210_ADC_AUTOMUTE_REG13, AUTOMUTE13_DISABLE)) return err;
  }

  // 4. Enable stereo ALC on ADC1/2 (leave ADC3/4 untouched so any AEC reference stays fixed-gain)
  if (EspError err = write_reg(ES7210_ALC_SEL_REG16, ALC16_ADC12_STEREO)) return err;

  ESP_LOGI(TAG, "ES7210 ALC enabled (automute=%s)", enable_automute ? "on" : "off");
  return ESP_OK;
}

EspResult<> Es7210::on_set_hardware_gain(float gain_db) {
  return set_mic_gain(gain_from_db(gain_db));
}

EspResult<> Es7210::on_set_hardware_volume(int8_t volume_db) {
  return set_digital_volume(volume_db);
}

EspResult<> Es7210::init_default(uint8_t i2c_address) {
  EspResult<I2CDevice> dev_res = I2CMaster::instance().add_device(i2c_address, 100000);
  if (!dev_res) return dev_res.strip().log_error(TAG, "Failed to attach ES7210 to I2C bus");

  Es7210Config es_cfg = {};
  es_cfg.sample_rate_hz = config_.sample_rate;
  es_cfg.mclk_ratio = 256;
  es_cfg.mic_gain = gain_from_db(config_.default_gain_db);
  es_cfg.tdm_enable = config_.tdm_enable;

  switch (config_.bits_per_sample) {
    case I2S_DATA_BIT_WIDTH_16BIT:
      es_cfg.resolution = Es7210Resolution::Res16;
      break;
    case I2S_DATA_BIT_WIDTH_24BIT:
      es_cfg.resolution = Es7210Resolution::Res24;
      break;
    case I2S_DATA_BIT_WIDTH_32BIT:
      es_cfg.resolution = Es7210Resolution::Res32;
      break;
    default:
      ESP_LOGW(TAG, "Unsupported BITS_PER_SAMPLE %d", config_.bits_per_sample);
      return ESP_ERR_INVALID_ARG;
  }

  if (EspError err = start(std::move(*dev_res), es_cfg)) {
    return err.log(TAG, "Failed to initialize ES7210 configuration");
  }

  set_mic_mute(false);
  set_digital_volume(config_.default_volume_db);

  if (config_.aec_ref_channel >= 1 && config_.aec_ref_channel <= 4) {
    // Keep the speaker AEC reference channel at 0 dB analog + 0 dB digital so it never clips
    set_channel_gain(config_.aec_ref_channel, Es7210MicGain::Gain0dB);
    set_channel_volume(config_.aec_ref_channel, 0);
    // Power down unused MIC4 (Slot 3)
    if (config_.aec_ref_channel != 4) {
      set_channel_enabled(4, false);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(10));
  return ESP_OK;
}

}  // namespace halpp