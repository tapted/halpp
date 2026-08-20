#include "halpp/display/st7789.hpp"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_st7789.h>
#include <esp_log.h>

#include "halpp/config.hpp"

namespace halpp {

static const char* TAG = "St7789";

EspResult<void> St7789::init_default_spi(spi_host_device_t spi_host) {
  auto& inst = default_instance();
  if (inst.is_initialized()) return ESP_OK;

  // 1. Configure the SPI IO layer for the ST7789
  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = config::SpiBus::PIN_CHIP_SELECT,
      .dc_gpio_num = config::Display::PIN_DATA_COMMAND,
      .spi_mode = 0,
      .pclk_hz = config::SpiBus::SPI_CLK_WRITE_HZ,
      .trans_queue_depth = 10,
      .on_color_trans_done = Display::on_color_trans_done,
      .user_ctx = &inst,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .flags = {},
  };

  esp_lcd_panel_io_handle_t io_handle = nullptr;

  if (EspError err = esp_lcd_new_panel_io_spi(spi_host, &io_config, &io_handle)) {
    return err.log(TAG, "Failed to create SPI IO handle");
  }

  // Inject into the base class configuration
  inst.config_ = Config{
      .width = config::Display::WIDTH,                    // 172
      .height = config::Display::HEIGHT,                  // 320
      .bits_per_pixel = config::Display::BITS_PER_PIXEL,  // RGB565
      .io_handle = io_handle,
      .owns_io_handle = true,
  };

  return inst.begin();
}

EspResult<void> St7789::begin() {
  if (!config_.io_handle) return ESP_ERR_INVALID_STATE;
  if (is_initialized()) return ESP_OK;

  esp_lcd_panel_dev_config_t panel_config = {
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // Replaces LCD_RGB_ENDIAN_BGR
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG,      // Standard for 16-bit SPI
      .bits_per_pixel = config::Display::BITS_PER_PIXEL,
      .reset_gpio_num = config::Display::PIN_RESET,
      .vendor_config = nullptr,
      .flags =
          {
              .reset_active_high = 0,
          },
  };

  if (EspError err = esp_lcd_new_panel_st7789(config_.io_handle, &panel_config, &panel_handle_)) {
    return err.log(TAG, "Failed to create ST7789 panel handle");
  }

  if (EspError err = esp_lcd_panel_reset(panel_handle_)) return err;
  if (EspError err = esp_lcd_panel_init(panel_handle_)) return err;

  // Most IPS ST7789 panels are hardware-inverted by default
  invert(config::Display::INVERT_COLORS);
  swap_xy(config::Display::SWAP_XY);  // Apply the vendor X-Mirroring
  esp_lcd_panel_mirror(panel_handle_, true, false);

  // The 172x320 Magic Offset
  if (config_.width == 172) {
    esp_lcd_panel_set_gap(panel_handle_, 34, 0);
  }

  clear();

  if (EspError err = esp_lcd_panel_disp_on_off(panel_handle_, true)) return err;

  ESP_LOGI(TAG, "ST7789 Initialized via Native IDF (%dx%d)", config_.width, config_.height);
  return ESP_OK;
}

}  // namespace halpp