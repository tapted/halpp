#include "halpp/display/display.hpp"

#include <algorithm>
#include <cstring>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

namespace HAL {

// static
bool Display::on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                  esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
  return false;  // No high-priority task woken up by this callback.
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

EspResult<void> Display::draw_bitmap(int x_start, int y_start, int width, int height,
                                     const void* color_data) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_draw_bitmap(panel_handle_, x_start, y_start, x_start + width,
                                   y_start + height, color_data);
}

EspResult<void> Display::set_display_state(bool on) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_disp_on_off(panel_handle_, on);
}

EspResult<void> Display::invert(bool inverted) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_invert_color(panel_handle_, inverted);
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

}  // namespace HAL