/**
 * @file display.hpp
 * @brief Universal base class for esp_lcd compatible displays.
 */

#pragma once

#include <cstdint>

#include "espbase/esp_result.hpp"

struct esp_lcd_panel_io_t;
struct esp_lcd_panel_t;
typedef struct esp_lcd_panel_io_t *esp_lcd_panel_io_handle_t;
typedef struct esp_lcd_panel_t *esp_lcd_panel_handle_t;       

namespace HAL {

class Display {
 public:
  struct Config {
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t bits_per_pixel = 0;
    esp_lcd_panel_io_handle_t io_handle = nullptr;
    bool owns_io_handle = false;
  };

  Display() = default;
  explicit Display(Config config) : config_(config) {}
  virtual ~Display() { reset(); }

  // Strict ownership semantics
  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;
  Display(Display&& other) noexcept;
  Display& operator=(Display&& other) noexcept;

  bool is_initialized() const { return panel_handle_ != nullptr; }
  uint16_t width() const { return config_.width; }
  uint16_t height() const { return config_.height; }

  // --- Universal Drawing Primitives ---
  EspResult<void> reset();
  EspResult<void> draw_bitmap(int x_start, int y_start, int width, int height,
                              const void* color_data);
  EspResult<void> set_display_state(bool on);
  EspResult<void> invert(bool inverted);

  // Fills a rectangle safely using a fixed-size DMA chunking buffer
  EspResult<void> fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint32_t color);
  EspResult<void> clear();

 protected:
  Config config_;
  esp_lcd_panel_handle_t panel_handle_ = nullptr;
};

}  // namespace HAL