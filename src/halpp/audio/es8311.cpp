#include "halpp/audio/es8311.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace halpp {

namespace {

struct CoeffDiv {
  uint32_t mclk;     /* mclk frequency */
  uint32_t rate;     /* sample rate */
  uint8_t pre_div;   /* the pre divider with range from 1 to 8 */
  uint8_t pre_multi; /* the pre multiplier with 0: 1x, 1: 2x, 2: 4x, 3: 8x selection */
  uint8_t adc_div;   /* adcclk divider */
  uint8_t dac_div;   /* dacclk divider */
  uint8_t fs_mode;   /* double speed or single speed, =0, ss, =1, ds */
  uint8_t lrck_h;    /* adclrck divider and daclrck divider */
  uint8_t lrck_l;
  uint8_t bclk_div; /* sclk divider */
  uint8_t adc_osr;  /* adc osr */
  uint8_t dac_osr;  /* dac osr */
};

static constexpr CoeffDiv coeff_table[] = {
    /*!<mclk     rate   pre_div  mult  adc_div dac_div fs_mode lrch  lrcl  bckdiv osr */
    /* 8k */
    {12288000, 8000, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 8000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x10},
    {16384000, 8000, 0x08, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 8000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 8000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 8000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 8000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 11.025k */
    {11289600, 11025, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 11025, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 11025, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 12k */
    {12288000, 12000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 12000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 12000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 16k */
    {12288000, 16000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 16000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 16000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 16000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 16000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 22.05k */
    {11289600, 22050, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 22050, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {705600, 22050, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 24k */
    {12288000, 24000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 24000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 32k */
    {12288000, 32000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 32000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 32000, 0x03, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {1024000, 32000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 44.1k */
    {11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 44100, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 48k */
    {12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 48000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 64k */
    {12288000, 64000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 64000, 0x03, 0x02, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {16384000, 64000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 64000, 0x01, 0x02, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {4096000, 64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 64000, 0x01, 0x03, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {2048000, 64000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 64000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0xbf, 0x03, 0x18, 0x18},
    {1024000, 64000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

    /* 88.2k */
    {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 88200, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 88200, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

    /* 96k */
    {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 96000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 96000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 96000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
};

static const CoeffDiv* get_coeff(uint32_t mclk, uint32_t rate) {
  for (const auto& coeff : coeff_table) {
    if (coeff.rate == rate && coeff.mclk == mclk) {
      return &coeff;
    }
  }
  return nullptr;
}

}  // namespace

void Es8311::reset() {
  i2c_dev_.reset();  // Automatically calls i2c_master_bus_rm_device
}

EspResult<void> Es8311::write_reg(uint8_t reg_addr, uint8_t data) {
  return i2c_dev_.write_reg(reg_addr, data);
}

EspResult<void> Es8311::read_reg(uint8_t reg_addr, uint8_t& data) {
  uint8_t buf[1];
  if (EspError err = i2c_dev_.read_reg(reg_addr, buf)) return err;
  data = buf[0];

  return ESP_OK;
}

EspResult<void> Es8311::start(I2CDevice i2c_dev, const Es8311Config& config) {
  if (!i2c_dev) return ESP_ERR_INVALID_ARG;
  if (i2c_dev_) return ESP_ERR_INVALID_STATE;

  if (config.sample_frequency < 8000 || config.sample_frequency > 96000) {
    ESP_LOGE("Es8311", "ES8311 init needs frequency in interval [8000; 96000] Hz");
    return ESP_ERR_INVALID_ARG;
  }

  i2c_dev_ = std::move(i2c_dev);

  // 1. Reset Sequence
  if (EspError err = write_reg(0x00, 0x1F)) return err;
  vTaskDelay(pdMS_TO_TICKS(20));
  if (EspError err = write_reg(0x00, 0x00)) return err;
  if (EspError err = write_reg(0x00, 0x80)) return err;

  // 2. Clock & Format
  if (EspError err = config_clock(config)) return err;
  if (EspError err = config_format(config.resolution)) return err;

  // 3. Power up analog circuitry
  if (EspError err = write_reg(0x0D, 0x01)) return err;
  if (EspError err = write_reg(0x0E, 0x02)) return err;
  if (EspError err = write_reg(0x12, 0x00)) return err;
  if (EspError err = write_reg(0x13, 0x10)) return err;
  if (EspError err = write_reg(0x1C, 0x6A)) return err;
  if (EspError err = write_reg(0x37, 0x08)) return err;

  return ESP_OK;
}

EspResult<void> Es8311::config_clock(const Es8311Config& config) {
  uint8_t reg06;
  uint8_t reg01 = 0x3F;  // Enable all clocks

  /* Select clock source for internal MCLK and determine its frequency */
  uint32_t mclk_hz = config.mclk_frequency;
  if (!config.mclk_from_mclk_pin) {
    mclk_hz = config.sample_frequency * static_cast<int>(config.resolution) * 2;
    reg01 |= (1 << 7);  // Select BCLK (a.k.a. SCK) pin
  }

  if (config.mclk_inverted) reg01 |= (1 << 6);  // Invert MCLK pin
  if (EspError err = write_reg(0x01, reg01)) return err;

  if (EspError err = read_reg(0x06, reg06)) return err;
  if (config.sclk_inverted) {
    reg06 |= (1 << 5);
  } else {
    reg06 &= ~(1 << 5);
  }
  if (EspError err = write_reg(0x06, reg06)) return err;

  /* Configure clock dividers */
  return config_sample_frequency(mclk_hz, config.sample_frequency);
}

EspResult<void> Es8311::config_sample_frequency(uint32_t mclk, uint32_t sample_rate) {
  const CoeffDiv* coeff = get_coeff(mclk, sample_rate);
  if (!coeff) {
    ESP_LOGE("Es8311", "Unable to configure sample rate %dHz with %dHz MCLK", sample_rate, mclk);
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t regv;
  if (EspError err = read_reg(0x02, regv)) return err;
  regv &= 0x07;
  regv |= (coeff->pre_div - 1) << 5;
  regv |= coeff->pre_multi << 3;
  if (EspError err = write_reg(0x02, regv)) return err;

  if (EspError err = write_reg(0x03, (coeff->fs_mode << 6) | coeff->adc_osr)) return err;
  if (EspError err = write_reg(0x04, coeff->dac_osr)) return err;
  if (EspError err = write_reg(0x05, ((coeff->adc_div - 1) << 4) | (coeff->dac_div - 1)))
    return err;

  if (EspError err = read_reg(0x06, regv)) return err;
  regv &= 0xE0;
  regv |= (coeff->bclk_div < 19) ? (coeff->bclk_div - 1) : coeff->bclk_div;
  if (EspError err = write_reg(0x06, regv)) return err;

  if (EspError err = read_reg(0x07, regv)) return err;
  regv &= 0xC0;
  regv |= coeff->lrck_h;
  if (EspError err = write_reg(0x07, regv)) return err;
  if (EspError err = write_reg(0x08, coeff->lrck_l)) return err;

  return ESP_OK;
}

EspResult<void> Es8311::config_format(Es8311Resolution res) {
  uint8_t reg00;
  if (EspError err = read_reg(0x00, reg00)) return err;
  reg00 &= 0xBF;  // Slave mode
  if (EspError err = write_reg(0x00, reg00)) return err;

  uint8_t res_bits = 0;
  switch (res) {
    case Es8311Resolution::Res16:
      res_bits = (3 << 2);
      break;
    case Es8311Resolution::Res18:
      res_bits = (2 << 2);
      break;
    case Es8311Resolution::Res20:
      res_bits = (1 << 2);
      break;
    case Es8311Resolution::Res24:
      res_bits = (0 << 2);
      break;
    case Es8311Resolution::Res32:
      res_bits = (4 << 2);
      break;
  }

  /* Setup SDP In and Out resolution */
  if (EspError err = write_reg(0x09, res_bits)) return err;  // SDP IN
  if (EspError err = write_reg(0x0A, res_bits)) return err;  // SDP OUT
  return ESP_OK;
}

EspResult<void> Es8311::set_voice_volume(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  int reg32 = (percent == 0) ? 0 : ((percent * 256) / 100) - 1;
  return write_reg(0x32, reg32);
}

EspResult<void> Es8311::set_voice_mute(bool mute) {
  uint8_t reg31;
  if (EspError err = read_reg(0x31, reg31)) return err;
  if (mute) {
    reg31 |= (1 << 6) | (1 << 5);
  } else {
    reg31 &= ~((1 << 6) | (1 << 5));
  }
  return write_reg(0x31, reg31);
}

EspResult<void> Es8311::set_voice_fade(Es8311Fade fade) {
  uint8_t reg37;
  if (EspError err = read_reg(0x37, reg37)) return err;
  reg37 = (reg37 & 0x0F) | (static_cast<uint8_t>(fade) << 4);
  return write_reg(0x37, reg37);
}

EspResult<void> Es8311::configure_microphone(bool digital_mic) {
  uint8_t reg14 = 0x1A;                                  // enable analog MIC and max PGA gain
  if (digital_mic) reg14 |= (1 << 6);                    // PDM digital microphone enable or disable
  if (EspError err = write_reg(0x17, 0xC8)) return err;  // Set ADC Gain.
  return write_reg(0x14, reg14);
}

EspResult<void> Es8311::configure_alc(bool enable) {
  if (!enable) {
    // Read REG18, clear bit 7 to disable ALC, write it back
    uint8_t reg18;
    if (EspError err = read_reg(0x18, reg18)) return err;
    reg18 &= ~(1 << 7);
    return write_reg(0x18, reg18);
  }

  // -----------------------------------------------------------
  // 1. REG 0x18: ALC Enable & Target Level
  // Bit 7: ALC Enable (1)
  // Bits 6:4: Target Level (e.g., 101 = -7.5dBFS)
  // Bits 3:0: Hold time (Wait time before increasing gain)
  // -----------------------------------------------------------
  // 0xDA = 1101 1010
  if (EspError err = write_reg(0x18, 0xDA)) return err;

  // -----------------------------------------------------------
  // 2. REG 0x19: Max / Min Gain Limits
  // We strictly limit the maximum gain. If we let it go to max (+30dB+),
  // the mic will turn into a vacuum cleaner during silence and trigger feedback.
  // Bits 7:5: Max Gain
  // Bits 4:0: Min Gain
  // -----------------------------------------------------------
  // 0x50 restricts the maximum gain heavily while allowing it to drop low.
  if (EspError err = write_reg(0x19, 0x50)) return err;

  // -----------------------------------------------------------
  // 3. REG 0x1A: Automute / Noise Gate (Crucial for Walkie-Talkies)
  // If the ambient volume drops below a threshold, the chip physically
  // mutes the ADC data stream. This murders feedback loops.
  // Bits 7:5: Automute Threshold
  // Bit 4: Automute Enable (1)
  // -----------------------------------------------------------
  // 0x14 = Enable Automute with a moderate noise threshold
  if (EspError err = write_reg(0x1A, 0x14)) return err;

  // -----------------------------------------------------------
  // 4. REG 0x1B: Attack and Decay Times
  // How fast the ALC reacts. We want a FAST attack (instantly clamp
  // down on sudden feedback squeals) and a moderate decay.
  // Bits 7:4: Attack Time
  // Bits 3:0: Decay Time
  // -----------------------------------------------------------
  // 0x34 = Fast attack, moderate decay
  if (EspError err = write_reg(0x1B, 0x34)) return err;

  ESP_LOGI("Es8311", "ES8311 ALC Voice Profile Enabled");
  return ESP_OK;
}

}  // namespace halpp

// Registers reference.
/*
 *   ES8311_REGISTER NAME_REG_REGISTER ADDRESS
 */
#define ES8311_RESET_REG00 0x00 /*reset digital,csm,clock manager etc.*/

/*
 * Clock Scheme Register definition
 */
#define ES8311_CLK_MANAGER_REG01 0x01 /* select clk src for mclk, enable clock for codec */
#define ES8311_CLK_MANAGER_REG02 0x02 /* clk divider and clk multiplier */
#define ES8311_CLK_MANAGER_REG03 0x03 /* adc fsmode and osr  */
#define ES8311_CLK_MANAGER_REG04 0x04 /* dac osr */
#define ES8311_CLK_MANAGER_REG05 0x05 /* clk divier for adc and dac */
#define ES8311_CLK_MANAGER_REG06 0x06 /* bclk inverter and divider */
#define ES8311_CLK_MANAGER_REG07 0x07 /* tri-state, lrck divider */
#define ES8311_CLK_MANAGER_REG08 0x08 /* lrck divider */
/*
 * SDP
 */
#define ES8311_SDPIN_REG09 0x09  /* dac serial digital port */
#define ES8311_SDPOUT_REG0A 0x0A /* adc serial digital port */
/*
 * SYSTEM
 */
#define ES8311_SYSTEM_REG0B 0x0B /* system */
#define ES8311_SYSTEM_REG0C 0x0C /* system */
#define ES8311_SYSTEM_REG0D 0x0D /* system, power up/down */
#define ES8311_SYSTEM_REG0E 0x0E /* system, power up/down */
#define ES8311_SYSTEM_REG0F 0x0F /* system, low power */
#define ES8311_SYSTEM_REG10 0x10 /* system */
#define ES8311_SYSTEM_REG11 0x11 /* system */
#define ES8311_SYSTEM_REG12 0x12 /* system, Enable DAC */
#define ES8311_SYSTEM_REG13 0x13 /* system */
#define ES8311_SYSTEM_REG14 0x14 /* system, select DMIC, select analog pga gain */
/*
 * ADC
 */
#define ES8311_ADC_REG15 0x15 /* ADC, adc ramp rate, dmic sense */
#define ES8311_ADC_REG16 0x16 /* ADC */
#define ES8311_ADC_REG17 0x17 /* ADC, volume */
#define ES8311_ADC_REG18 0x18 /* ADC, alc enable and winsize */
#define ES8311_ADC_REG19 0x19 /* ADC, alc maxlevel */
#define ES8311_ADC_REG1A 0x1A /* ADC, alc automute */
#define ES8311_ADC_REG1B 0x1B /* ADC, alc automute, adc hpf s1 */
#define ES8311_ADC_REG1C 0x1C /* ADC, equalizer, hpf s2 */
/*
 * DAC
 */
#define ES8311_DAC_REG31 0x31 /* DAC, mute */
#define ES8311_DAC_REG32 0x32 /* DAC, volume */
#define ES8311_DAC_REG33 0x33 /* DAC, offset */
#define ES8311_DAC_REG34 0x34 /* DAC, drc enable, drc winsize */
#define ES8311_DAC_REG35 0x35 /* DAC, drc maxlevel, minilevel */
#define ES8311_DAC_REG37 0x37 /* DAC, ramprate */
/*
 *GPIO
 */
#define ES8311_GPIO_REG44 0x44 /* GPIO, dac2adc for test */
#define ES8311_GP_REG45 0x45   /* GP CONTROL */
/*
 * CHIP
 */
#define ES8311_CHD1_REGFD 0xFD  /* CHIP ID1 */
#define ES8311_CHD2_REGFE 0xFE  /* CHIP ID2 */
#define ES8311_CHVER_REGFF 0xFF /* VERSION */
#define ES8311_CHD1_REGFD 0xFD  /* CHIP ID1 */

#define ES8311_MAX_REGISTER 0xFF
