#include "halpp/display/ssd1306.hpp"

#include <cstring>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_ssd1306.h>
#include <esp_log.h>
#include <lvgl.h>

#include "halpp/display/boot_logo.hpp"
#include "halpp/i2c/i2c_master.hpp"

namespace HAL {

static const char* TAG = "Ssd1306";

EspResult<void> Ssd1306::init_default_i2c(uint8_t i2c_address, uint16_t width, uint16_t height) {
  auto& inst = default_instance();
  if (inst.is_initialized()) return ESP_OK;

  // 1. Configure the I2C IO layer specifics for the SSD1306
  esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = i2c_address,
      .scl_speed_hz = 400000,  // 400 kHz is the maximum for SSD1306
      .control_phase_bytes = 1,
      .dc_bit_offset = 6,  // Crucial: Tells the IO layer where the Data/Command bit lives
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .on_color_trans_done = Display::on_color_trans_done,
      .user_ctx = &inst,
      .flags =
          {
              .dc_low_on_data = 0,
              .disable_control_phase = 0,
          },
  };

  esp_lcd_panel_io_handle_t io_handle = nullptr;

  // Link the display IO to halpp's modern I2C Master bus
  if (EspError err = esp_lcd_new_panel_io_i2c(I2CMaster::instance().get_bus_handle(), &io_config,
                                              &io_handle)) {
    return err.log(TAG, "Failed to create I2C IO handle");
  }

  // Inject into the base class configuration. Note the bits_per_pixel!
  inst.config_ = Config{
      .width = width,
      .height = height,
      .bits_per_pixel = 1,
      .io_handle = io_handle,
      .owns_io_handle = true,
  };

  return inst.begin();
}

EspResult<void> Ssd1306::begin() {
  if (!config_.io_handle) return ESP_ERR_INVALID_STATE;
  if (is_initialized()) return ESP_OK;
  esp_lcd_panel_ssd1306_config_t ssd1306_config = {.height = 64};
  switch (config_.height) {
    case 32:
      ssd1306_config.height = 32;
      break;
    case 64:
      ssd1306_config.height = 64;  // Default, but explicit for clarity.
      break;
    default:
      ESP_LOGE(TAG, "Unsupported SSD1306 height: %d. Only 32 or 64 are supported.", config_.height);
      return ESP_ERR_INVALID_ARG;
  }

  esp_lcd_panel_dev_config_t panel_config = {
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // Ignored by monochrome SSD1306
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG,      // Ignored by monochrome SSD1306
      .bits_per_pixel = 1,
      // I2C modules typically don't have a hardware reset pin wired
      .reset_gpio_num = GPIO_NUM_NC,
      .vendor_config = &ssd1306_config,
      .flags =
          {
              .reset_active_high = 0,
          },
  };

  if (EspError err = esp_lcd_new_panel_ssd1306(config_.io_handle, &panel_config, &panel_handle_)) {
    return err.log(TAG, "Failed to create SSD1306 panel handle");
  }

  // Reset will likely be a no-op because the reset GPIO is not connected.
  if (EspError err = esp_lcd_panel_reset(panel_handle_)) return err;

  if (EspError err = esp_lcd_panel_init(panel_handle_)) return err;

  invert(true);  // Invert in hardware for now. We maybe need to read the lvgl palette properly.

  // Ensure RAM doesn't show static on boot
  if (config_.width == 128 && config_.height == 64) {
    // draw_bitmap(0, 0, config_.width, config_.height, Assets::BOOT_LOGO.data());
    // Boot logo is already transposed.
    esp_lcd_panel_draw_bitmap(panel_handle_, 0, 0, config_.width, config_.height,
                              Assets::BOOT_LOGO.data());
  } else {
    clear();
  }

  if (EspError err = esp_lcd_panel_disp_on_off(panel_handle_, true)) return err;  // 100ms delay.

  ESP_LOGI(TAG, "SSD1306 Initialized (%dx%d)", config_.width, config_.height);
  return ESP_OK;
}

EspResult<void> Ssd1306::draw_bitmap(int x, int y, int w, int h, const void* color_data,
                                     uint32_t stride) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;

  // If no stride was provided (e.g. standard user call), assume tightly packed
  if (stride == 0) stride = (w + 7) / 8;

  const int pages = (h + 7) / 8;
  const size_t tx_size = w * pages;

  // Ensure our DMA-capable transpose buffer is large enough for this specific window
  if (tx_buffer_size_ < tx_size) {
    if (tx_buffer_) heap_caps_free(tx_buffer_);
    tx_buffer_ =
        static_cast<uint8_t*>(heap_caps_malloc(tx_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    tx_buffer_size_ = tx_size;
    if (!tx_buffer_) return ESP_ERR_NO_MEM;
  }

  std::memset(tx_buffer_, 0, tx_size);
  const uint8_t* src = static_cast<const uint8_t*>(color_data);

  // The mathematically flawless Transpose (Horizontal LVGL -> Vertical SSD1306)
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      // Read from the padded LVGL buffer
      int src_byte_idx = (row * stride) + (col / 8);
      int src_bit_idx = 7 - (col % 8);  // LVGL LV_COLOR_FORMAT_I1 puts leftmost pixel in MSB

      if (src[src_byte_idx] & (1 << src_bit_idx)) {
        // Write to the vertical SSD1306 page buffer
        int dst_byte_idx = (row / 8) * w + col;
        int dst_bit_idx = row % 8;  // SSD1306 puts topmost pixel in LSB
        tx_buffer_[dst_byte_idx] |= (1 << dst_bit_idx);
      }
    }
  }

  // Hand the perfectly transposed buffer to the ESP-IDF driver
  return esp_lcd_panel_draw_bitmap(panel_handle_, x, y, x + w, y + h, tx_buffer_);
}

void Ssd1306::on_lvgl_init(lv_display_t* disp) {
  // SSD1306 hardware physically cannot draw arbitrary Y coordinates.
  // It must draw in chunks of 8 pixels (pages).
  lv_display_add_event_cb(
      disp,
      [](lv_event_t* e) {
        lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(e));
        if (area) {
          area->y1 = area->y1 & ~0x7;   // Round down to nearest 8
          area->y2 = (area->y2 | 0x7);  // Round up to nearest 8
        }
      },
      LV_EVENT_INVALIDATE_AREA, nullptr);
}

}  // namespace HAL