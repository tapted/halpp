#include "halpp/display/generic_display.hpp"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>

#include "halpp/config.hpp"
#include "halpp/display/boot_logo.hpp"

static constexpr const char TAG[] = "GenericDisplay";

namespace halpp {
namespace detail {
esp_err_t not_supported_new_panel_func(const esp_lcd_panel_io_handle_t,
                                       const esp_lcd_panel_dev_config_t*, esp_lcd_panel_handle_t*) {
  ESP_LOGE(TAG, "No NEW_PANEL_FUNC defined in halpp::config::Display");
  return ESP_ERR_NOT_SUPPORTED;
}

static void draw_default_color_boot_logo(Display& display) {
  constexpr uint16_t WIDTH = config::Display::WIDTH;
  constexpr uint16_t HEIGHT = config::Display::HEIGHT;
  constexpr uint16_t logo_size = Assets::COLOR_LOGO_SIZE;

  // Screen coordinates (pads edges if screen is larger than logo)
  const uint16_t target_x = (WIDTH > logo_size) ? (WIDTH - logo_size) / 2 : 0;
  const uint16_t target_y = (HEIGHT > logo_size) ? (HEIGHT - logo_size) / 2 : 0;

  // Source crop offsets (centers crop if screen is smaller than logo)
  const uint16_t src_x = (WIDTH < logo_size) ? (logo_size - WIDTH) / 2 : 0;
  const uint16_t src_y = (HEIGHT < logo_size) ? (logo_size - HEIGHT) / 2 : 0;

  // Visible width and height clamped to hardware screen dimensions
  constexpr uint16_t draw_w = std::min<uint16_t>(logo_size, WIDTH);
  constexpr uint16_t draw_h = std::min<uint16_t>(logo_size, HEIGHT);

  constexpr uint32_t bg_color = Assets::COLOR_LOGO_BG_COLOR;

  // Fill negative space only where space actually exists
  if (target_y > 0) {
    display.fill_rect(0, 0, WIDTH, target_y, bg_color);  // Top
  }
  if (HEIGHT > target_y + logo_size) {
    display.fill_rect(0, target_y + logo_size, WIDTH, HEIGHT - target_y - logo_size,
                      bg_color);  // Bottom
  }
  if (target_x > 0) {
    display.fill_rect(0, target_y, target_x, draw_h, bg_color);  // Left
  }
  if (WIDTH > target_x + logo_size) {
    display.fill_rect(target_x + logo_size, target_y, WIDTH - target_x - logo_size, draw_h,
                      bg_color);  // Right
  }

  if (draw_w == logo_size) {
    display.draw_bitmap(target_x, target_y, target_x + draw_w, target_y + draw_h,
                        halpp::Assets::COLOR_LOGO.data());
  } else {
    display.draw_bitmap_2d(target_x, target_y, draw_w, draw_h,  // Target rect on screen
                           halpp::Assets::COLOR_LOGO.data(),    // Source buffer
                           logo_size, logo_size,                // Source total size (128x128)
                           src_x, src_y, draw_w, draw_h         // Source crop position & size
    );
  }
}

static void draw_default_monochrome_boot_logo(Display& display) {
  // Directly writes the pre-transposed monochrome logo to prevent static on boot
  if constexpr (config::Display::WIDTH == Assets::MONOCHROME_LOGO_WIDTH &&
                config::Display::HEIGHT == Assets::MONOCHROME_LOGO_HEIGHT) {
    esp_lcd_panel_draw_bitmap(display.get_panel_handle(), 0, 0, config::Display::WIDTH,
                              config::Display::HEIGHT, Assets::MONOCHROME_BOOT_LOGO.data());
  } else {
    display.clear();
  }
}

void draw_default_boot_logo(Display& display) {
  if constexpr (config::Display::BITS_PER_PIXEL == 1) {
    draw_default_monochrome_boot_logo(display);
  } else {
    draw_default_color_boot_logo(display);
  }
}

}  // namespace detail

EspResult<void> GenericDisplay::begin() {
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

  config::Display::BOOT_LOGO_FUNC(*this);
  if (EspError err = esp_lcd_panel_disp_on_off(panel_handle_, true)) return err;

  if (config::Display::PIN_BACKLIGHT_PWM != GPIO_NUM_NC) {
    if (EspError err = backlight_.begin()) {
      return err.log(TAG, "Failed to initialize backlight");
    }
    backlight_.set_level(config::Display::BACKLIGHT_DEFAULT);
  }

  ESP_LOGI(TAG, "Display Initialized via Native IDF (%dx%d)", config_.width, config_.height);
  return ESP_OK;
}

}  // namespace halpp