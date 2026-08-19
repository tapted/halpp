#pragma once

#include <cstdint>
#include <driver/i2s_types.h>
#include <hal/i2s_types.h>
#include <hal/ledc_types.h>
#include <hal/spi_types.h>
#include <soc/gpio_num.h>

namespace halpp::detail {
struct SharedDefaults {
  struct System {
    static constexpr gpio_num_t PIN_BOOT = GPIO_NUM_0;  // Boot mode control strapping pin
  };
  struct SpiBus {
    static constexpr spi_host_device_t SPI_HOST = SPI2_HOST;
    static constexpr uint32_t SPI_CLK_WRITE_HZ = 80 * 1000 * 1000;  // 80MHz for write

    static constexpr gpio_num_t PIN_CHIP_SELECT = GPIO_NUM_NC;   // Chip Select (CS)
    static constexpr gpio_num_t PIN_SERIAL_CLOCK = GPIO_NUM_NC;  // Serial Clock (SCK)

    // Serial Interface Data
    static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_NC;  // Master Out Slave In (MOSI)

    // QSPI Data Lanes
    static constexpr gpio_num_t PIN_QSPI_SDA_0 = GPIO_NUM_NC;
    static constexpr gpio_num_t PIN_QSPI_SDA_1 = GPIO_NUM_NC;  // Strapping pin: VDD_SPI voltage
    static constexpr gpio_num_t PIN_QSPI_SDA_2 = GPIO_NUM_NC;
    static constexpr gpio_num_t PIN_QSPI_SDA_3 = GPIO_NUM_NC;
  };
  struct Usb {
    static constexpr gpio_num_t PIN_USB_DM = GPIO_NUM_NC;   // "Native" USB D- (Data Minus)
    static constexpr gpio_num_t PIN_USB_DP = GPIO_NUM_NC;   // "Native" USB D+ (Data Plus)
    static constexpr gpio_num_t PIN_UART_TX = GPIO_NUM_NC;  // USB-UART TX (to PC)
    static constexpr gpio_num_t PIN_UART_RX = GPIO_NUM_NC;  // USB-UART RX (from PC)
  };
  struct I2CConfig {
    static constexpr gpio_num_t PIN_SDA = GPIO_NUM_NC;
    static constexpr gpio_num_t PIN_SCL = GPIO_NUM_NC;

    static constexpr uint8_t BUS_NUM = 0;          // I2C_NUM_0
    static constexpr uint32_t CLK_SPEED = 400000;  // 400kHz standard
    static constexpr uint32_t TIMEOUT_MS = 1000;   // Transaction timeout
    static constexpr bool ENABLE_PULLUP = true;
    static constexpr uint32_t SCL_WAIT_US = 0;  // 0 = use default
  };
  struct Display {
    static constexpr gpio_num_t PIN_TEARING_EFFECT = GPIO_NUM_NC;  // Tearing Effect (TE)
    static constexpr gpio_num_t PIN_BACKLIGHT_PWM = GPIO_NUM_NC;   // Backlight PWM control

    static constexpr uint8_t BITS_PER_PIXEL = 16;
    static constexpr bool INVERT_COLORS = false;  // Invert colors (e.g. for OLEDs - white on black)
    static constexpr bool SWAP_XY = false;        // Swap X/Y for portrait vs landscape

    static constexpr ledc_channel_t BACKLIGHT_LEDC_CHANNEL = LEDC_CHANNEL_0;
    static constexpr ledc_timer_t BACKLIGHT_LEDC_TIMER = LEDC_TIMER_0;
    static constexpr uint32_t BACKLIGHT_LEDC_FREQ = 5000;
    static constexpr ledc_timer_bit_t BACKLIGHT_LEDC_RESOLUTION = LEDC_TIMER_13_BIT;
    static constexpr uint8_t BACKLIGHT_MAX = 100;  // Max backlight level (0-100)
  };
  struct Touch {
    static constexpr uint8_t I2C_ADDRESS = 0x15;              // Default I2C address for CST816S
    static constexpr gpio_num_t PIN_INTERRUPT = GPIO_NUM_NC;  // Touch interrupt pin
  };
  struct Display7Seg {
    static constexpr uint8_t I2C_ADDRESS = 0x70;  // Default I2C address for HT16K33
  };
  struct lvgl {
    static constexpr bool DOUBLE_BUFFERED = true;   // Use two buffers for LVGL rendering
    static constexpr uint32_t BUFFER_FRACTION = 1;  // Buffer size = screen_pixels / buffer_fraction
    static constexpr uint32_t TASK_STACK_SIZE = 8192;  // LVGL task stack size
    static constexpr uint32_t TASK_PRIORITY = 5;       // LVGL task priority
    static constexpr uint8_t TASK_CORE_ID = 0;         // LVGL task core affinity
  };
  struct Buzzer {
    static constexpr gpio_num_t PIN_PWM = GPIO_NUM_NC;  // PWM output for passive buzzer
  };
  struct IndicatorLed {
    static constexpr gpio_num_t PIN_RGB = GPIO_NUM_NC;  // RGB output for indicator LED
  };
  struct SdCard {
    static constexpr gpio_num_t PIN_SERIAL_CLOCK = GPIO_NUM_NC;  // SD Clock
    static constexpr gpio_num_t PIN_MISO = GPIO_NUM_NC;  // SD Serial Data (Master In Slave Out) D0
    static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_NC;  // SD Serial Command (Master Out Slave In)
    static constexpr gpio_num_t PIN_CHIP_SELECT = GPIO_NUM_NC;  // SD Chip Select
    static constexpr gpio_num_t PIN_DATA1 = GPIO_NUM_NC;        // SD Data Line 1 (D1)
    static constexpr gpio_num_t PIN_DATA2 = GPIO_NUM_NC;        // SD Data Line 2 (D2)
  };
  struct Audio {
    static constexpr uint32_t SAMPLE_RATE = 48000;             // Default sample rate for I2S
    static constexpr uint8_t DEFAULT_VOLUME = 70;              // Default volume percentage (0-100)
    static constexpr gpio_num_t PIN_AMP_ENABLE = GPIO_NUM_NC;  // GPIO to enable the amplifier

    static constexpr gpio_num_t PIN_DATA_IN = GPIO_NUM_NC;   // I2S Data In (from Mic)
    static constexpr gpio_num_t PIN_DATA_OUT = GPIO_NUM_NC;  // I2S Data Out (to Speaker)
    static constexpr gpio_num_t PIN_BCK = GPIO_NUM_NC;       // I2S Bit Clock
    static constexpr gpio_num_t PIN_WS = GPIO_NUM_NC;        // I2S Word Select (LRCK)
    static constexpr gpio_num_t PIN_MCLK = GPIO_NUM_NC;      // I2S Master Clock (MCLK)

    static constexpr uint8_t I2S_PORT = I2S_NUM_0;  // I2S port number
    static constexpr i2s_data_bit_width_t BITS_PER_SAMPLE = I2S_DATA_BIT_WIDTH_16BIT;
    static constexpr i2s_slot_mode_t SLOT_MODE = I2S_SLOT_MODE_STEREO;

    static constexpr uint8_t SPEAKER_I2C_ADDRESS = 0x18;  // Default I2C address for ES8311

    // Default to stereo. Usually the mic and speaker channels need to match in i2s. If you have
    // two mics, this will need to be true even if you have a mono speaker.
    static constexpr bool IS_STEREO = true;
    static constexpr uint8_t MAX_MIXER_STREAMS = 4;
  };
};  // SharedDefaults

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
struct Defaults : public SharedDefaults {
  struct SpiBus : public SharedDefaults::SpiBus {
    static constexpr gpio_num_t PIN_CHIP_SELECT = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_SERIAL_CLOCK = GPIO_NUM_40;

    // Data Lanes
    static constexpr gpio_num_t PIN_QSPI_SDA_0 = GPIO_NUM_46;
    static constexpr gpio_num_t PIN_QSPI_SDA_1 = GPIO_NUM_45;
    static constexpr gpio_num_t PIN_QSPI_SDA_2 = GPIO_NUM_42;
    static constexpr gpio_num_t PIN_QSPI_SDA_3 = GPIO_NUM_41;
  };
  struct Usb : public SharedDefaults::Usb {
    static constexpr gpio_num_t PIN_USB_DM = GPIO_NUM_19;
    static constexpr gpio_num_t PIN_USB_DP = GPIO_NUM_20;
    static constexpr gpio_num_t PIN_UART_TX = GPIO_NUM_43;
    static constexpr gpio_num_t PIN_UART_RX = GPIO_NUM_44;
  };
  struct I2CConfig : public SharedDefaults::I2CConfig {
    static constexpr gpio_num_t PIN_SDA = GPIO_NUM_11;
    static constexpr gpio_num_t PIN_SCL = GPIO_NUM_10;
  };
  struct Display : public SharedDefaults::Display {
    static constexpr gpio_num_t PIN_TEARING_EFFECT = GPIO_NUM_18;
    static constexpr gpio_num_t PIN_BACKLIGHT_PWM = GPIO_NUM_5;
  };
  struct Touch : public SharedDefaults::Touch {
    static constexpr gpio_num_t PIN_INTERRUPT = GPIO_NUM_4;
  };
  struct lvgl : public SharedDefaults::lvgl {
    static constexpr uint8_t TASK_CORE_ID = 1;
  };
  struct Buzzer : public SharedDefaults::Buzzer {
    static constexpr gpio_num_t PIN_PWM = GPIO_NUM_13;
  };
  struct Audio : public SharedDefaults::Audio {
    static constexpr gpio_num_t PIN_AMP_ENABLE = GPIO_NUM_15;

    static constexpr gpio_num_t PIN_DATA_IN = GPIO_NUM_39;
    static constexpr gpio_num_t PIN_DATA_OUT = GPIO_NUM_47;
    static constexpr gpio_num_t PIN_BCK = GPIO_NUM_48;
    static constexpr gpio_num_t PIN_WS = GPIO_NUM_38;
    static constexpr gpio_num_t PIN_MCLK = GPIO_NUM_2;
  };
};  // Defaults
#endif
#if CONFIG_IDF_TARGET_ESP32C6
struct Defaults : public SharedDefaults {
  struct SpiBus : public SharedDefaults::SpiBus {
    static constexpr gpio_num_t PIN_CHIP_SELECT = GPIO_NUM_14;
    static constexpr gpio_num_t PIN_SERIAL_CLOCK = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_6;
  };
  struct Usb : public SharedDefaults::Usb {
    static constexpr gpio_num_t PIN_UART_TX = GPIO_NUM_16;
    static constexpr gpio_num_t PIN_UART_RX = GPIO_NUM_17;
  };
  struct I2CConfig : public SharedDefaults::I2CConfig {
    static constexpr gpio_num_t PIN_SDA = GPIO_NUM_11;
    static constexpr gpio_num_t PIN_SCL = GPIO_NUM_10;
  };
  struct Display : public SharedDefaults::Display {
    static constexpr gpio_num_t PIN_DATA_COMMAND = GPIO_NUM_15;
    static constexpr gpio_num_t PIN_RESET = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_BACKLIGHT_PWM = GPIO_NUM_22;
  };
  struct IndicatorLed : public SharedDefaults::IndicatorLed {
    static constexpr gpio_num_t PIN_RGB = GPIO_NUM_8;
  };
  struct SdCard : public SharedDefaults::SdCard {
    static constexpr gpio_num_t PIN_SERIAL_CLOCK = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_MISO = GPIO_NUM_5;
    static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_5;
    static constexpr gpio_num_t PIN_CHIP_SELECT = GPIO_NUM_4;
  };
};  // Defaults
#endif
}  // namespace halpp::detail
