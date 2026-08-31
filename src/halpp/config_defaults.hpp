#pragma once

#include <cstdint>
#include <driver/i2s_types.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_types.h>
#include <hal/i2s_types.h>
#include <hal/ledc_types.h>
#include <hal/spi_types.h>
#include <soc/gpio_num.h>

namespace halpp {
class Display;
}

namespace halpp::detail {

esp_err_t not_supported_new_panel_func(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_panel_dev_config_t* panel_dev_config,
                                       esp_lcd_panel_handle_t* ret_panel);
void draw_default_boot_logo(halpp::Display& display);

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
    static constexpr gpio_num_t PIN_MISO = GPIO_NUM_NC;  // Master In Slave Out (MISO)

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
    static constexpr gpio_num_t PIN_DATA_COMMAND = GPIO_NUM_NC;    // Data/Command (DC)
    static constexpr gpio_num_t PIN_RESET = GPIO_NUM_NC;           // Reset (RST)
    static constexpr gpio_num_t PIN_BACKLIGHT_PWM = GPIO_NUM_NC;   // Backlight PWM control

    static constexpr uint8_t SPI_COMMAND_BITS = 8;
    static constexpr uint8_t SPI_PARAM_BITS = 8;

    static constexpr uint8_t BITS_PER_PIXEL = 16;
    static constexpr lcd_rgb_element_order_t RGB_ELEMENT_ORDER = LCD_RGB_ELEMENT_ORDER_RGB;
    static constexpr lcd_rgb_data_endian_t DATA_ENDIAN = LCD_RGB_DATA_ENDIAN_BIG;
    static constexpr auto NEW_PANEL_FUNC = not_supported_new_panel_func;
    static constexpr auto BOOT_LOGO_FUNC = draw_default_boot_logo;
    static constexpr void* VENDOR_CONFIG = nullptr;  // Vendor-specific configuration, if needed

    // Skip hardware reset during initialization (e.g., if reset is done via EXIO, not GPIO).
    static constexpr bool SKIP_RESET = false;
    static constexpr bool INVERT_COLORS = false;  // Invert colors (e.g. for OLEDs - white on black)
    static constexpr bool SWAP_XY = false;        // Swap X/Y for portrait vs landscape
    static constexpr bool MIRROR_X = false;       // Mirror X axis (horizontal flip)
    static constexpr bool MIRROR_Y = false;       // Mirror Y axis (vertical flip)
    static constexpr uint16_t X_GAP = 0;          // Horizontal gap (offset)
    static constexpr uint16_t Y_GAP = 0;          // Vertical gap (offset)

    enum class ClockSource { AUTO, PLL /* c6 only */, RTC, XTAL };
    static constexpr ClockSource BACKLIGHT_CLOCK_SOURCE = ClockSource::RTC;
    static constexpr ledc_channel_t BACKLIGHT_LEDC_CHANNEL = LEDC_CHANNEL_0;
    static constexpr ledc_timer_t BACKLIGHT_LEDC_TIMER = LEDC_TIMER_0;
    static constexpr uint32_t BACKLIGHT_LEDC_FREQ = 3000;
    static constexpr ledc_timer_bit_t BACKLIGHT_LEDC_RESOLUTION = LEDC_TIMER_12_BIT;
    static constexpr uint8_t BACKLIGHT_DEFAULT = 30;  // Backlight level set on boot
    static constexpr uint8_t BACKLIGHT_MAX = 100;     // Max backlight level (0-100)
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

    static constexpr bool USE_MAIN_LOOP = true;  // Use espbase main_loop, not a freertos task.
    static constexpr bool USE_RGB565_SWAPPED = false;  // LV_COLOR_FORMAT_RGB565_SWAPPED for 16bit
    static constexpr uint32_t TASK_STACK_SIZE = 8192;  // LVGL freertos task stack size
    static constexpr uint32_t TASK_PRIORITY = 5;       // LVGL freertos task priority
    static constexpr uint8_t TASK_CORE_ID = 0;         // LVGL freertos task core affinity
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
    static constexpr gpio_num_t PIN_MISO = GPIO_NUM_5;  // Shared with SdCard
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
    static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_6;
    static constexpr gpio_num_t PIN_CHIP_SELECT = GPIO_NUM_4;
  };
};  // Defaults
#endif
}  // namespace halpp::detail
