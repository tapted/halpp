#include "halpp/display/display.hpp"

#include <algorithm>
#include <cstring>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <lvgl.h>

#include "halpp/config.hpp"

static const char* TAG = "HAL::Display";

using LVGLConfig = HAL::config::lvgl;

namespace HAL {

// static
// --- LVGL 9 ISR Callback ---
IRAM_ATTR bool Display::on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                            esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
  Display* display = static_cast<Display*>(user_ctx);
  if (display && display->lv_display_) {
    lv_display_flush_ready(display->lv_display_);
  }
  return false;
}

static uint32_t halpp_lvgl_tick_cb(void) {
  // Explicitly cast to uint32_t to leverage LVGL's internal math for safe unsigned wrapping.
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

// --- LVGL 9 Initialization ---
EspResult<void> Display::init_lvgl() {
  if (!is_initialized()) return ESP_ERR_INVALID_STATE;
  if (lv_display_) return ESP_OK;  // Already initialized

  // Defensively ensure the LVGL core is initialized.
  if (!lv_is_initialized()) lv_init();
  lv_tick_set_cb(halpp_lvgl_tick_cb);

  // 1. Calculate Buffer Size dynamically via halpp_board.hpp overrides
  constexpr size_t LVGL_OVERHEAD = 128;  // sizeof(lv_draw_buf_t)
  const uint32_t pixels_per_screen = config_.width * config_.height;
  const uint32_t buffer_pixels = pixels_per_screen / LVGLConfig::BUFFER_FRACTION;
  const size_t buffer_bytes = (buffer_pixels * config_.bits_per_pixel + 7) / 8 + LVGL_OVERHEAD;

  // 2. Allocate DMA-capable memory
  void* buf1 = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* buf2 = nullptr;

  if (LVGLConfig::DOUBLE_BUFFERED) {
    buf2 = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  }

  if (!buf1 || (LVGLConfig::DOUBLE_BUFFERED && !buf2)) {
    ESP_LOGE(TAG, "Failed to allocate LVGL DMA buffers!");
    return ESP_ERR_NO_MEM;
  }

  // 3. Create LVGL 9 Display Object
  lv_display_ = lv_display_create(config_.width, config_.height);
  if (!lv_display_) return ESP_ERR_NO_MEM;

  // Link our C++ class instance to the LVGL object
  lv_display_set_user_data(lv_display_, this);

  // 4. Dynamically set color format based on our hardware config
  if (config_.bits_per_pixel == 1) {
    // Perfect for SSD1306! Tells LVGL to pack pixels into 1-bit boundaries.
    lv_display_set_color_format(lv_display_, LV_COLOR_FORMAT_I1);
  } else if (config_.bits_per_pixel == 16) {
    lv_display_set_color_format(lv_display_, LV_COLOR_FORMAT_RGB565);
  } else {
    ESP_LOGW(TAG, "Unsupported BPP for LVGL auto-config");
  }

  on_lvgl_init(lv_display_);  // Hook for subclasses.

  // 5. Assign buffers (LVGL 9 expects size in BYTES, not pixels!)
  lv_display_set_buffers(lv_display_, buf1, buf2, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // 6. Set up the flush callback using the new v9 signature
  lv_display_set_flush_cb(
      lv_display_, [](lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
        Display* display = static_cast<Display*>(lv_display_get_user_data(disp));

        const int w = area->x2 - area->x1 + 1;
        const int h = area->y2 - area->y1 + 1;

        // We explicitly ask LVGL what the padded stride is for this specific area update
        uint32_t lv_stride = lv_draw_buf_width_to_stride(w, lv_display_get_color_format(disp));

        // px_map is now a raw byte pointer in v9, perfectly matching our draw_bitmap signature
        display->draw_bitmap(area->x1, area->y1, w, h, px_map, lv_stride);

        // We do NOT call lv_display_flush_ready here, because the DMA engine
        // will trigger on_color_trans_done() when the hardware actually finishes!
      });

  ESP_LOGI(TAG, "LVGL 9 Display initialized (Buffer: %zu bytes)", buffer_bytes);
  return ESP_OK;
}

EspResult<void> Display::reset() {
  esp_err_t final_err = ESP_OK;

  if (panel_handle_) {
    final_err = esp_lcd_panel_del(panel_handle_);
    panel_handle_ = nullptr;
  }

  if (config_.owns_io_handle && config_.io_handle) {
    esp_err_t io_err = esp_lcd_panel_io_del(config_.io_handle);
    if (final_err == ESP_OK) final_err = io_err;
    config_.io_handle = nullptr;
  }

  return final_err;
}

EspResult<void> Display::set_display_state(bool on) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_disp_on_off(panel_handle_, on);
}

EspResult<void> Display::invert(bool inverted) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_invert_color(panel_handle_, inverted);
}

EspResult<void> Display::swap_xy(bool swap) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_swap_xy(panel_handle_, swap);
}

EspResult<void> Display::fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                                   uint32_t color) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;

  // 1. Calculate bytes needed for a single line
  uint32_t bytes_per_line = (w * config_.bits_per_pixel + 7) / 8;
  if (bytes_per_line == 0) return ESP_ERR_INVALID_ARG;

  // 2. Fixed, stack-safe buffer (1KB max).
  // For a 128x64 1-bit OLED, this fits the ENTIRE SCREEN at once.
  // For a 320x240 16-bit TFT, this safely chunks line-by-line.
  constexpr size_t MAX_BUFFER_BYTES = 1024;
  if (bytes_per_line > MAX_BUFFER_BYTES)
    return ESP_ERR_NO_MEM;  // Line is too wide for stack buffer

  alignas(4) uint8_t chunk_buffer[MAX_BUFFER_BYTES];
  uint16_t lines_per_chunk = MAX_BUFFER_BYTES / bytes_per_line;

  // 3. Format the chunk buffer based on the display's color depth
  if (config_.bits_per_pixel == 1) {
    uint8_t c = (color == 0) ? 0x00 : 0xFF;
    std::memset(chunk_buffer, c, MAX_BUFFER_BYTES);
  } else if (config_.bits_per_pixel == 16) {
    uint16_t* buf16 = reinterpret_cast<uint16_t*>(chunk_buffer);
    for (size_t i = 0; i < MAX_BUFFER_BYTES / 2; ++i) {
      buf16[i] = static_cast<uint16_t>(color);
    }
  } else {
    return ESP_ERR_NOT_SUPPORTED;  // Fallback for unsupported BPP
  }

  // 4. Draw the rectangles in safe batches
  for (uint16_t y = y0; y < y0 + h; y += lines_per_chunk) {
    uint16_t lines_to_draw = std::min(lines_per_chunk, static_cast<uint16_t>(y0 + h - y));
    if (EspError err = esp_lcd_panel_draw_bitmap(panel_handle_, x0, y, x0 + w, y + lines_to_draw,
                                                 chunk_buffer)) {
      return err;
    }
  }

  return ESP_OK;
}

EspResult<void> Display::clear() {
  return fill_rect(0, 0, config_.width, config_.height, 0);
}

EspResult<void> Display::draw_bitmap(int x_start, int y_start, int width, int height,
                                     const void* color_data, uint32_t /*stride_bytes*/) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;

  // Base class expects color_data to be directly compatible with the esp_lcd driver.
  // We ignore the stride here because esp_lcd expects contiguous bounding boxes.
  return esp_lcd_panel_draw_bitmap(panel_handle_, x_start, y_start, x_start + width,
                                   y_start + height, color_data);
}

void Display::on_lvgl_init(lv_display_t*) {
}

}  // namespace HAL