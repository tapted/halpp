#include "halpp/audio/es8311.hpp"

#include <cinttypes>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "halpp/i2c/i2c_master.hpp"

namespace halpp {

namespace {

constexpr const char TAG[] = "Es8311";

// I2C & Clock Defaults
constexpr uint8_t ES8311_ADDRESS_0 = 0x18;  // CE pin low
constexpr uint8_t ES8311_ADDRESS_1 = 0x19;  // CE pin high
constexpr uint32_t ES8311_I2C_FREQ_HZ = 100000;
constexpr uint32_t ES8311_MCLK_RATIO = 256;
constexpr uint32_t ES8311_MIN_SAMPLE_RATE_HZ = 8000;
constexpr uint32_t ES8311_MAX_SAMPLE_RATE_HZ = 96000;
constexpr uint32_t ES8311_RESET_DELAY_MS = 20;
constexpr uint32_t ES8311_STABILIZE_DELAY_MS = 20;

// Reset & State Machine Registers
constexpr uint8_t ES8311_RESET_REG00 = 0x00;  // Reset digital, CSM, clock manager

// Clock Scheme Registers
constexpr uint8_t ES8311_CLK_MANAGER_REG01 = 0x01;  // Select clock src for MCLK, enable clocks
constexpr uint8_t ES8311_CLK_MANAGER_REG02 = 0x02;  // Clock divider and clock multiplier
constexpr uint8_t ES8311_CLK_MANAGER_REG03 = 0x03;  // ADC fs mode and OSR
constexpr uint8_t ES8311_CLK_MANAGER_REG04 = 0x04;  // DAC OSR
constexpr uint8_t ES8311_CLK_MANAGER_REG05 = 0x05;  // Clock divider for ADC and DAC
constexpr uint8_t ES8311_CLK_MANAGER_REG06 = 0x06;  // BCLK inverter and divider
constexpr uint8_t ES8311_CLK_MANAGER_REG07 = 0x07;  // Tri-state, LRCK divider high
constexpr uint8_t ES8311_CLK_MANAGER_REG08 = 0x08;  // LRCK divider low

// Serial Digital Port (SDP) Registers
constexpr uint8_t ES8311_SDPIN_REG09 = 0x09;   // DAC serial digital port
constexpr uint8_t ES8311_SDPOUT_REG0A = 0x0A;  // ADC serial digital port

// System Registers
constexpr uint8_t ES8311_SYSTEM_REG0B = 0x0B;  // System
constexpr uint8_t ES8311_SYSTEM_REG0C = 0x0C;  // System
constexpr uint8_t ES8311_SYSTEM_REG0D = 0x0D;  // System power up/down (VMID, VREF, analog)
constexpr uint8_t ES8311_SYSTEM_REG0E = 0x0E;  // System power up/down (PGA, modulator)
constexpr uint8_t ES8311_SYSTEM_REG0F = 0x0F;  // System low power
constexpr uint8_t ES8311_SYSTEM_REG10 = 0x10;  // System
constexpr uint8_t ES8311_SYSTEM_REG11 = 0x11;  // System
constexpr uint8_t ES8311_SYSTEM_REG12 = 0x12;  // System enable/power-down DAC
constexpr uint8_t ES8311_SYSTEM_REG13 = 0x13;  // System output driver control
constexpr uint8_t ES8311_SYSTEM_REG14 = 0x14;  // Select DMIC, analog PGA gain

// ADC Registers
constexpr uint8_t ES8311_ADC_REG15 = 0x15;  // ADC ramp rate, DMIC sense
constexpr uint8_t ES8311_ADC_REG16 = 0x16;  // ADC
constexpr uint8_t ES8311_ADC_REG17 = 0x17;  // ADC volume
constexpr uint8_t ES8311_ADC_REG18 = 0x18;  // ALC enable and window size
constexpr uint8_t ES8311_ADC_REG19 = 0x19;  // ALC max/min level
constexpr uint8_t ES8311_ADC_REG1A = 0x1A;  // ALC automute
constexpr uint8_t ES8311_ADC_REG1B = 0x1B;  // ALC automute, ADC HPF stage 1
constexpr uint8_t ES8311_ADC_REG1C = 0x1C;  // ADC equalizer, HPF stage 2

// DAC Registers
constexpr uint8_t ES8311_DAC_REG31 = 0x31;  // DAC mute
constexpr uint8_t ES8311_DAC_REG32 = 0x32;  // DAC volume
constexpr uint8_t ES8311_DAC_REG33 = 0x33;  // DAC offset
constexpr uint8_t ES8311_DAC_REG34 = 0x34;  // DAC DRC enable, DRC window size
constexpr uint8_t ES8311_DAC_REG35 = 0x35;  // DAC DRC max/min level
constexpr uint8_t ES8311_DAC_REG37 = 0x37;  // DAC ramp rate

// GPIO & Chip ID Registers
constexpr uint8_t ES8311_GPIO_REG44 = 0x44;   // GPIO, DAC-to-ADC test
constexpr uint8_t ES8311_GP_REG45 = 0x45;     // GP control
constexpr uint8_t ES8311_CHD1_REGFD = 0xFD;   // Chip ID1
constexpr uint8_t ES8311_CHD2_REGFE = 0xFE;   // Chip ID2
constexpr uint8_t ES8311_CHVER_REGFF = 0xFF;  // Chip version

// REG 0x00: Reset & CSM Bitfields
constexpr uint8_t RESET00_STANDBY = 0x00;
constexpr uint8_t RESET00_SOFT_RESET = 0x1F;
constexpr uint8_t RESET00_MASTER_MODE = 1u << 6;
constexpr uint8_t RESET00_CSM_ON = 1u << 7;

// REG 0x01 - 0x07: Clock Manager Bitfields
constexpr uint8_t CLK01_DISABLE_ALL = 0x00;
constexpr uint8_t CLK01_ENABLE_ALL = 0x3F;
constexpr uint8_t CLK01_MCLK_INVERT = 1u << 6;
constexpr uint8_t CLK01_MCLK_FROM_BCLK = 1u << 7;
constexpr uint8_t CLK02_PRESERVE_MASK = 0x07;
constexpr uint8_t CLK06_BCLK_INVERT = 1u << 5;
constexpr uint8_t CLK06_PRESERVE_MASK = 0xE0;
constexpr uint8_t CLK06_BCLK_DIV_DIRECT_THRESHOLD = 19;
constexpr uint8_t CLK07_PRESERVE_MASK = 0xC0;

// REG 0x09 / 0x0A: SDP Word Length Bitfields
constexpr uint8_t SDP_RES_24BIT = 0u << 2;
constexpr uint8_t SDP_RES_20BIT = 1u << 2;
constexpr uint8_t SDP_RES_18BIT = 2u << 2;
constexpr uint8_t SDP_RES_16BIT = 3u << 2;
constexpr uint8_t SDP_RES_32BIT = 4u << 2;

// REG 0x0D - 0x14: System Power & Mic Bitfields
constexpr uint8_t SYS0D_POWER_UP_VMID = 0x01;  // Power up analog + VMID normal charge
constexpr uint8_t SYS0D_PDN_ALL = 0xFC;        // Power down VMID, VREF, biasgen, analog
constexpr uint8_t SYS0E_ENABLE_ANALOG = 0x02;  // Enable analog PGA/modulator
constexpr uint8_t SYS0E_PDN_ALL = 0xFF;        // Power down analog PGA/modulator
constexpr uint8_t SYS12_ENABLE_DAC = 0x00;     // PDN_DAC = 0
constexpr uint8_t SYS12_PDN_DAC = 0x02;        // PDN_DAC = 1
constexpr uint8_t SYS13_PDN_OUTPUT = 0x00;     // Disable output driver
constexpr uint8_t SYS13_ENABLE_OUTPUT = 0x10;  // Enable output driver
constexpr uint8_t SYS14_PDN_MIC_PGA = 0x00;    // Disable MIC bias & PGA
constexpr uint8_t SYS14_ANALOG_MIC_MAX_PGA = 0x1A;
constexpr uint8_t SYS14_DMIC_ENABLE = 1u << 6;

// REG 0x17 - 0x1C: ADC & ALC Bitfields
constexpr uint8_t ADC17_GAIN_DEFAULT = 0xC8;
constexpr uint8_t ADC18_ALC_ENABLE = 1u << 7;
constexpr uint8_t ADC18_ALC_VOICE_PROFILE = 0xDA;  // ALC enabled, -7.5 dBFS target
constexpr uint8_t ADC19_ALC_GAIN_LIMITS = 0x50;    // Clamp max gain to prevent feedback
constexpr uint8_t ADC1A_AUTOMUTE_VOICE = 0x14;     // Enable noise gate automute
constexpr uint8_t ADC1B_ALC_ATTACK_DECAY = 0x34;   // Fast attack, moderate decay
constexpr uint8_t ADC1C_EQ_HPF_DEFAULT = 0x6A;

// REG 0x31 - 0x37: DAC Mute, Volume & Ramp Bitfields
constexpr uint8_t DAC31_SOFT_MUTE = 1u << 6;
constexpr uint8_t DAC31_HARD_MUTE = 1u << 5;
constexpr uint8_t DAC31_MUTE_MASK = DAC31_SOFT_MUTE | DAC31_HARD_MUTE;
constexpr uint8_t DAC32_VOL_MIN = 0x00;  // -95.5 dB
constexpr uint8_t DAC37_RAMPRATE_DEFAULT = 0x08;
constexpr uint8_t DAC37_RAMPRATE_MASK = 0x0F;

struct CoeffDiv {
  uint32_t mclk;      // MCLK frequency (Hz)
  uint32_t rate;      // Sample rate (Hz)
  uint8_t pre_div;    // Pre-divider [1..8]
  uint8_t pre_multi;  // Pre-multiplier (0: 1x, 1: 2x, 2: 4x, 3: 8x)
  uint8_t adc_div;    // ADC clock divider
  uint8_t dac_div;    // DAC clock divider
  uint8_t fs_mode;    // 0: single-speed, 1: double-speed
  uint8_t lrck_h;     // ADC/DAC LRCK divider high bits
  uint8_t lrck_l;     // ADC/DAC LRCK divider low bits
  uint8_t bclk_div;   // SCLK/BCLK divider
  uint8_t adc_osr;    // ADC oversampling ratio
  uint8_t dac_osr;    // DAC oversampling ratio
};

constexpr CoeffDiv coeff_table[] = {
    // mclk     rate   pre_div  mult  adc_div dac_div fs_mode lrch  lrcl  bckdiv osr
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

const CoeffDiv* get_coeff(uint32_t mclk, uint32_t rate) {
  for (const auto& coeff : coeff_table) {
    if (coeff.rate == rate && coeff.mclk == mclk) {
      return &coeff;
    }
  }
  return nullptr;
}

}  // namespace

EspResult<> Es8311::reset() {
  if (i2c_dev_) {
    set_voice_mute(true);  // Mute before shutdown to prevent pops
  }
  return i2c_dev_.reset();  // Automatically calls i2c_master_bus_rm_device
}

EspResult<> Es8311::powerdown() {
  if (!i2c_dev_) return ESP_ERR_INVALID_STATE;

  // 1. Mute DAC and drop digital volume to minimum (-95.5 dB)
  if (EspError err = set_voice_mute(true)) return err;
  if (EspError err = write_reg(ES8311_DAC_REG32, DAC32_VOL_MIN)) return err;

  // 2. Power down DAC, output driver, and analog MIC/PGA
  if (EspError err = write_reg(ES8311_SYSTEM_REG12, SYS12_PDN_DAC)) return err;
  if (EspError err = write_reg(ES8311_SYSTEM_REG13, SYS13_PDN_OUTPUT)) return err;
  if (EspError err = write_reg(ES8311_SYSTEM_REG14, SYS14_PDN_MIC_PGA)) return err;

  // 3. Power down analog blocks, VREF, bias generator, and VMID
  if (EspError err = write_reg(ES8311_SYSTEM_REG0E, SYS0E_PDN_ALL)) return err;
  if (EspError err = write_reg(ES8311_SYSTEM_REG0D, SYS0D_PDN_ALL)) return err;

  // 4. Gate internal clocks and disable the Chip State Machine (CSM_ON = 0)
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG01, CLK01_DISABLE_ALL)) return err;
  if (EspError err = write_reg(ES8311_RESET_REG00, RESET00_STANDBY)) return err;

  return i2c_dev_.reset();
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

  if (config.sample_frequency < ES8311_MIN_SAMPLE_RATE_HZ ||
      config.sample_frequency > ES8311_MAX_SAMPLE_RATE_HZ) {
    ESP_LOGE(TAG, "ES8311 init needs frequency in interval [%" PRIu32 "; %" PRIu32 "] Hz",
             ES8311_MIN_SAMPLE_RATE_HZ, ES8311_MAX_SAMPLE_RATE_HZ);
    return ESP_ERR_INVALID_ARG;
  }

  i2c_dev_ = std::move(i2c_dev);

  // 1. Reset Sequence
  if (EspError err = write_reg(ES8311_RESET_REG00, RESET00_SOFT_RESET)) return err;
  vTaskDelay(pdMS_TO_TICKS(ES8311_RESET_DELAY_MS));
  if (EspError err = write_reg(ES8311_RESET_REG00, RESET00_STANDBY)) return err;
  if (EspError err = write_reg(ES8311_RESET_REG00, RESET00_CSM_ON)) return err;

  // 2. Clock & Format
  if (EspError err = config_clock(config)) return err;
  if (EspError err = config_format(config.resolution)) return err;

  // 3. Power up analog circuitry
  if (EspError err = write_reg(ES8311_SYSTEM_REG0D, SYS0D_POWER_UP_VMID)) return err;
  if (EspError err = write_reg(ES8311_SYSTEM_REG0E, SYS0E_ENABLE_ANALOG)) return err;
  if (EspError err = write_reg(ES8311_SYSTEM_REG12, SYS12_ENABLE_DAC)) return err;
  if (EspError err = write_reg(ES8311_SYSTEM_REG13, SYS13_ENABLE_OUTPUT)) return err;
  if (EspError err = write_reg(ES8311_ADC_REG1C, ADC1C_EQ_HPF_DEFAULT)) return err;
  if (EspError err = write_reg(ES8311_DAC_REG37, DAC37_RAMPRATE_DEFAULT)) return err;

  return ESP_OK;
}

EspResult<void> Es8311::config_clock(const Es8311Config& config) {
  uint8_t reg06;
  uint8_t reg01 = CLK01_ENABLE_ALL;

  // Select clock source for internal MCLK and determine its frequency
  uint32_t mclk_hz = config.mclk_frequency;
  if (!config.mclk_from_mclk_pin) {
    mclk_hz = config.sample_frequency * static_cast<uint32_t>(config.resolution) * 2;
    reg01 |= CLK01_MCLK_FROM_BCLK;
  }

  if (config.mclk_inverted) reg01 |= CLK01_MCLK_INVERT;
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG01, reg01)) return err;

  if (EspError err = read_reg(ES8311_CLK_MANAGER_REG06, reg06)) return err;
  if (config.sclk_inverted) {
    reg06 |= CLK06_BCLK_INVERT;
  } else {
    reg06 &= ~CLK06_BCLK_INVERT;
  }
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG06, reg06)) return err;

  // Configure clock dividers
  return config_sample_frequency(mclk_hz, config.sample_frequency);
}

EspResult<void> Es8311::config_sample_frequency(uint32_t mclk, uint32_t sample_rate) {
  const CoeffDiv* coeff = get_coeff(mclk, sample_rate);
  if (!coeff) {
    ESP_LOGE(TAG, "Unable to configure sample rate %" PRIu32 "Hz with %" PRIu32 "Hz MCLK",
             sample_rate, mclk);
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t regv;
  if (EspError err = read_reg(ES8311_CLK_MANAGER_REG02, regv)) return err;
  regv &= CLK02_PRESERVE_MASK;
  regv |= static_cast<uint8_t>((coeff->pre_div - 1) << 5);
  regv |= static_cast<uint8_t>(coeff->pre_multi << 3);
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG02, regv)) return err;

  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG03,
                               static_cast<uint8_t>((coeff->fs_mode << 6) | coeff->adc_osr))) {
    return err;
  }
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG04, coeff->dac_osr)) return err;
  if (EspError err =
          write_reg(ES8311_CLK_MANAGER_REG05,
                    static_cast<uint8_t>(((coeff->adc_div - 1) << 4) | (coeff->dac_div - 1)))) {
    return err;
  }

  if (EspError err = read_reg(ES8311_CLK_MANAGER_REG06, regv)) return err;
  regv &= CLK06_PRESERVE_MASK;
  regv |=
      (coeff->bclk_div < CLK06_BCLK_DIV_DIRECT_THRESHOLD) ? (coeff->bclk_div - 1) : coeff->bclk_div;
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG06, regv)) return err;

  if (EspError err = read_reg(ES8311_CLK_MANAGER_REG07, regv)) return err;
  regv &= CLK07_PRESERVE_MASK;
  regv |= coeff->lrck_h;
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG07, regv)) return err;
  if (EspError err = write_reg(ES8311_CLK_MANAGER_REG08, coeff->lrck_l)) return err;

  return ESP_OK;
}

EspResult<void> Es8311::config_format(Es8311Resolution res) {
  uint8_t reg00;
  if (EspError err = read_reg(ES8311_RESET_REG00, reg00)) return err;
  reg00 &= ~RESET00_MASTER_MODE;  // Slave mode
  if (EspError err = write_reg(ES8311_RESET_REG00, reg00)) return err;

  uint8_t res_bits = 0;
  switch (res) {
    case Es8311Resolution::Res16:
      res_bits = SDP_RES_16BIT;
      break;
    case Es8311Resolution::Res18:
      res_bits = SDP_RES_18BIT;
      break;
    case Es8311Resolution::Res20:
      res_bits = SDP_RES_20BIT;
      break;
    case Es8311Resolution::Res24:
      res_bits = SDP_RES_24BIT;
      break;
    case Es8311Resolution::Res32:
      res_bits = SDP_RES_32BIT;
      break;
  }

  // Setup SDP In and Out resolution
  if (EspError err = write_reg(ES8311_SDPIN_REG09, res_bits)) return err;
  if (EspError err = write_reg(ES8311_SDPOUT_REG0A, res_bits)) return err;
  return ESP_OK;
}

EspResult<void> Es8311::set_voice_volume(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  const uint8_t reg32 =
      (percent == 0) ? DAC32_VOL_MIN : static_cast<uint8_t>(((percent * 256) / 100) - 1);
  return write_reg(ES8311_DAC_REG32, reg32);
}

EspResult<void> Es8311::set_voice_mute(bool mute) {
  uint8_t reg31;
  if (EspError err = read_reg(ES8311_DAC_REG31, reg31)) return err;
  if (mute) {
    reg31 |= DAC31_MUTE_MASK;
  } else {
    reg31 &= ~DAC31_MUTE_MASK;
  }
  return write_reg(ES8311_DAC_REG31, reg31);
}

EspResult<void> Es8311::set_voice_fade(Es8311Fade fade) {
  uint8_t reg37;
  if (EspError err = read_reg(ES8311_DAC_REG37, reg37)) return err;
  reg37 = (reg37 & DAC37_RAMPRATE_MASK) | (static_cast<uint8_t>(fade) << 4);
  return write_reg(ES8311_DAC_REG37, reg37);
}

EspResult<void> Es8311::configure_microphone(bool digital_mic) {
  uint8_t reg14 = SYS14_ANALOG_MIC_MAX_PGA;
  if (digital_mic) reg14 |= SYS14_DMIC_ENABLE;
  if (EspError err = write_reg(ES8311_ADC_REG17, ADC17_GAIN_DEFAULT)) return err;
  return write_reg(ES8311_SYSTEM_REG14, reg14);
}

EspResult<void> Es8311::configure_alc(bool enable) {
  if (!enable) {
    uint8_t reg18;
    if (EspError err = read_reg(ES8311_ADC_REG18, reg18)) return err;
    reg18 &= ~ADC18_ALC_ENABLE;
    return write_reg(ES8311_ADC_REG18, reg18);
  }

  // 1. Enable ALC, set target level (-7.5 dBFS) and hold time
  if (EspError err = write_reg(ES8311_ADC_REG18, ADC18_ALC_VOICE_PROFILE)) return err;

  // 2. Clamp max/min PGA gain limits to prevent acoustic feedback
  if (EspError err = write_reg(ES8311_ADC_REG19, ADC19_ALC_GAIN_LIMITS)) return err;

  // 3. Enable noise-gate automute below ambient threshold
  if (EspError err = write_reg(ES8311_ADC_REG1A, ADC1A_AUTOMUTE_VOICE)) return err;

  // 4. Configure fast attack and moderate decay times
  if (EspError err = write_reg(ES8311_ADC_REG1B, ADC1B_ALC_ATTACK_DECAY)) return err;

  ESP_LOGI(TAG, "ES8311 ALC Voice Profile Enabled");
  return ESP_OK;
}

EspResult<> Es8311::on_set_hardware_volume(uint8_t percent) {
  return set_voice_volume(percent);
}

EspResult<> Es8311::init_default(uint8_t i2c_address) {
  // 1. Provision the device on the I2C bus via the singleton
  EspResult<I2CDevice> dev_res = I2CMaster::instance().add_device(i2c_address, ES8311_I2C_FREQ_HZ);
  if (!dev_res) return dev_res.strip().log_error(TAG, "Failed to attach ES8311 to I2C bus");

  // 2. Configure the Audio parameters
  halpp::Es8311Config es_cfg = {};
  es_cfg.mclk_from_mclk_pin = true;
  es_cfg.mclk_frequency = config_.sample_rate * ES8311_MCLK_RATIO;
  es_cfg.sample_frequency = config_.sample_rate;
  switch (config_.bits_per_sample) {
    case I2S_DATA_BIT_WIDTH_16BIT:
      es_cfg.resolution = halpp::Es8311Resolution::Res16;
      break;
    case I2S_DATA_BIT_WIDTH_24BIT:
      es_cfg.resolution = halpp::Es8311Resolution::Res24;
      break;
    case I2S_DATA_BIT_WIDTH_32BIT:
      es_cfg.resolution = halpp::Es8311Resolution::Res32;
      break;
    default:
      ESP_LOGW(TAG, "Unsupported BITS_PER_SAMPLE %d", config_.bits_per_sample);
      return ESP_ERR_INVALID_ARG;
  }

  // 3. Inject the I2CDevice into the ES8311 module
  if (EspError err = start(std::move(*dev_res), es_cfg)) {
    return err.log(TAG, "Failed to initialize ES8311 configuration");
  }

  set_voice_fade(halpp::Es8311Fade::Fade64LRCK);
  configure_microphone(false);
  set_voice_mute(false);
  set_voice_volume(config_.default_volume);

  // Short safety delay to let the DAC stabilize
  vTaskDelay(pdMS_TO_TICKS(ES8311_STABILIZE_DELAY_MS));
  return ESP_OK;
}

}  // namespace halpp