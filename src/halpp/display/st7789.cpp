#include "halpp/display/st7789.hpp"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_st7789.h>
#include <esp_log.h>

#include "halpp/config.hpp"
#include "halpp/display/spi_init.hpp"

namespace halpp {

static const char* TAG = "St7789";

EspResult<void> St7789::init_default_spi() {
  auto& inst = default_instance();
  if (inst.is_initialized()) return ESP_OK;

  auto config = init_spi_display(&inst);
  if (!config) return config.strip().log_error(TAG, "init_spi_display");

  inst.config_ = *config;
  return inst.begin();
}

EspResult<void> St7789::begin() {
  if (!config_.io_handle) return ESP_ERR_INVALID_STATE;
  if (is_initialized()) return ESP_OK;

  esp_lcd_panel_dev_config_t panel_config = {
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // Replaces LCD_RGB_ENDIAN_BGR
      .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,      // Standard for 16-bit SPI
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

  invert(config::Display::INVERT_COLORS);
  swap_xy(config::Display::SWAP_XY);
  if (config::Display::MIRROR_X || config::Display::MIRROR_Y) {
    mirror(config::Display::MIRROR_X, config::Display::MIRROR_Y);
  }

  // The 172x320 Magic Offset
  if (config_.width == 172) {
    esp_lcd_panel_set_gap(panel_handle_, 34, 0);
  }

  if (EspError err = esp_lcd_panel_disp_on_off(panel_handle_, true)) return err;

  clear();  // TODO: boot logo

  if (EspError err = backlight_.begin()) {
    return err.log(TAG, "Failed to initialize backlight");
  }
  backlight_.set_level(config::Display::BACKLIGHT_DEFAULT);

  ESP_LOGI(TAG, "ST7789 Initialized via Native IDF (%dx%d)", config_.width, config_.height);
  return ESP_OK;
}

}  // namespace halpp