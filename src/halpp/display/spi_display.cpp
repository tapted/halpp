#include "halpp/display/spi_display.hpp"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>

#include "halpp/config.hpp"
#include "halpp/display/boot_logo.hpp"
#include "halpp/display/spi_init.hpp"

static constexpr const char TAG[] = "SpiDisplay";

namespace halpp {
namespace detail {
esp_err_t not_supported_new_panel_func(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_panel_dev_config_t* panel_dev_config,
                                       esp_lcd_panel_handle_t* ret_panel) {
  ESP_LOGE(TAG, "No NEW_PANEL_FUNC defined in halpp::config::Display");
  return ESP_ERR_NOT_SUPPORTED;
}

void draw_default_boot_logo(halpp::Display& display) {
  // Calculate dynamic centering for any display resolution
  const uint16_t WIDTH = config::Display::WIDTH;
  const uint16_t HEIGHT = config::Display::HEIGHT;
  const uint16_t logo_size = halpp::Assets::COLOR_LOGO_SIZE;
  const uint16_t x_off = (WIDTH - logo_size) / 2;
  const uint16_t y_off = (HEIGHT - logo_size) / 2;

  // Match the deep space background color from the asset generator
  const uint32_t bg_color = halpp::Assets::COLOR_LOGO_BG_COLOR;

  // Frame the negative space around the logo (top, bottom, left, right)
  display.fill_rect(0, 0, WIDTH, y_off, bg_color);
  display.fill_rect(0, y_off + logo_size, WIDTH, HEIGHT - y_off - logo_size, bg_color);
  display.fill_rect(0, y_off, x_off, logo_size, bg_color);
  display.fill_rect(x_off + logo_size, y_off, WIDTH - x_off - logo_size, logo_size, bg_color);

  // Draw the compile-time color logo perfectly in the center
  display.draw_bitmap(x_off, y_off, logo_size, logo_size, halpp::Assets::COLOR_LOGO.data());
}

}  // namespace detail

EspResult<void> SpiDisplay::init_default_spi() {
  auto& inst = default_instance();
  if (inst.is_initialized()) return ESP_OK;

  auto config = init_spi_display(&inst);
  if (!config) return config.strip().log_error(TAG, "init_spi_display");

  inst.config_ = *config;
  return inst.begin();
}

EspResult<void> SpiDisplay::begin() {
  if (!config_.io_handle) return ESP_ERR_INVALID_STATE;
  if (is_initialized()) return ESP_OK;

  esp_lcd_panel_dev_config_t panel_config = {
      .rgb_ele_order = config::Display::RGB_ELEMENT_ORDER,
      .data_endian = config::Display::DATA_ENDIAN,
      .bits_per_pixel = config::Display::BITS_PER_PIXEL,
      .reset_gpio_num = config::Display::PIN_RESET,
      .vendor_config = config::Display::VENDOR_CONFIG,
      .flags =
          {
              .reset_active_high = 0,
          },
  };
  if (EspError err =
          config::Display::NEW_PANEL_FUNC(config_.io_handle, &panel_config, &panel_handle_)) {
    return err.log(TAG, "Failed to create panel handle");
  }

  if (!config::Display::SKIP_RESET) {
    if (EspError err = esp_lcd_panel_reset(panel_handle_)) return err;
  }
  if (EspError err = esp_lcd_panel_init(panel_handle_)) return err;

  if (config::Display::INVERT_COLORS) invert(config::Display::INVERT_COLORS);
  if (config::Display::SWAP_XY) swap_xy(config::Display::SWAP_XY);
  if (config::Display::MIRROR_X || config::Display::MIRROR_Y) {
    mirror(config::Display::MIRROR_X, config::Display::MIRROR_Y);
  }
  if (config::Display::X_GAP != 0 || config::Display::Y_GAP != 0) {
    esp_lcd_panel_set_gap(panel_handle_, config::Display::X_GAP, config::Display::Y_GAP);
  }
  if (EspError err = esp_lcd_panel_disp_on_off(panel_handle_, true)) return err;

  config::Display::BOOT_LOGO_FUNC(*this);

  if (EspError err = backlight_.begin()) {
    return err.log(TAG, "Failed to initialize backlight");
  }
  backlight_.set_level(config::Display::BACKLIGHT_DEFAULT);

  ESP_LOGI(TAG, "SpiDisplay Initialized via Native IDF (%dx%d)", config_.width, config_.height);
  return ESP_OK;
}

}  // namespace halpp