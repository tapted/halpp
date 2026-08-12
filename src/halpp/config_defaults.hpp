#pragma once

#include <cstdint>
#include <driver/i2s_types.h>
#include <hal/i2s_types.h>
#include <soc/gpio_num.h>

namespace HAL::detail {
struct Defaults {
  struct I2CConfig {
    static constexpr gpio_num_t PIN_SDA = GPIO_NUM_8;
    static constexpr gpio_num_t PIN_SCL = GPIO_NUM_9;

    static constexpr uint8_t BUS_NUM = 0;          // I2C_NUM_0
    static constexpr uint32_t CLK_SPEED = 400000;  // 400kHz standard
    static constexpr uint32_t TIMEOUT_MS = 1000;   // Transaction timeout
    static constexpr bool ENABLE_PULLUP = true;
    static constexpr uint32_t SCL_WAIT_US = 0;  // 0 = use default
  };
  struct Display {
    static constexpr bool INVERT_COLORS = true;   // Invert colors (e.g. for OLEDs - white on black)
    static constexpr bool SWAP_XY = false;        // Swap X/Y for portrait vs landscape
    static constexpr bool TRANSPOSE_1BIT = true;  // Transpose displays for LVGL
  };
  struct Display7Seg {
    static constexpr uint8_t I2C_ADDRESS = 0x70;  // Default I2C address for HT16K33
  };
  struct lvgl {
    static constexpr bool DOUBLE_BUFFERED = true;   // Use two buffers for LVGL rendering
    static constexpr uint32_t BUFFER_FRACTION = 1;  // Buffer size = screen_pixels / buffer_fraction
    static constexpr uint32_t TASK_STACK_SIZE = 8192;  // LVGL task stack size
    static constexpr uint32_t TASK_PRIORITY = 5;       // LVGL task priority
    static constexpr uint8_t TASK_CORE_ID = 1;         // LVGL task core affinity
  };
  struct Audio {
    static constexpr uint32_t SAMPLE_RATE = 48000;             // Default sample rate for I2S
    static constexpr uint8_t DEFAULT_VOLUME = 70;              // Default volume percentage (0-100)
    static constexpr gpio_num_t PIN_AMP_ENABLE = GPIO_NUM_15;  // GPIO to enable the amplifier

    static constexpr gpio_num_t PIN_DATA_IN = GPIO_NUM_39;   // I2S Data In (from Mic)
    static constexpr gpio_num_t PIN_DATA_OUT = GPIO_NUM_47;  // I2S Data Out (to Speaker)
    static constexpr gpio_num_t PIN_BCK = GPIO_NUM_48;       // I2S Bit Clock
    static constexpr gpio_num_t PIN_WS = GPIO_NUM_38;        // I2S Word Select (LRCK)
    static constexpr gpio_num_t PIN_MCLK = GPIO_NUM_2;       // I2S Master Clock (MCLK)

    static constexpr uint8_t I2S_PORT = I2S_NUM_0;  // I2S port number
    static constexpr i2s_data_bit_width_t BITS_PER_SAMPLE = I2S_DATA_BIT_WIDTH_16BIT;
    static constexpr i2s_slot_mode_t SLOT_MODE = I2S_SLOT_MODE_STEREO;

    static constexpr uint8_t SPEAKER_I2C_ADDRESS = 0x18;  // Default I2C address for ES8311

    // Default to stereo. Usually the mic and speaker channels need to match in i2s. If you have
    // two mics, this will need to be true even if you have a mono speaker.
    static constexpr bool IS_STEREO = true;
    static constexpr uint8_t MAX_MIXER_STREAMS = 4;
  };
};  // Defaults
}  // namespace HAL::detail
