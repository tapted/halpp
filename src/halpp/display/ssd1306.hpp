/**
 * @file ssd1306.hpp
 * @brief SSD1306 specific initialization, inheriting from the generic Display class.
 */

#pragma once

#include "halpp/display/generic_display.hpp"

namespace halpp {

class Ssd1306 : public GenericDisplay {
 public:
  using GenericDisplay::GenericDisplay;  // Inherit constructors

  EspResult<> whole_display_on(bool enable);

  // Scroll the display horizontally at the specified speed. A scroll_speed of 0 disables scrolling.
  // Max scroll speed is 8 (corresponding to the fastest scroll interval in SCROLL_SPEED_MAP).
  EspResult<> horizontal_scroll(uint8_t scroll_speed);

  // --- Hardware Quirks Overrides ---
  uint8_t get_backlight() const override { return contrast_level_; }
  EspResult<> set_backlight(BacklightState state, uint8_t brightness, int fade_ms = 500) override;

  // SSD1306 requires vertical-page formatting. We intercept and transpose.
  EspResult<> draw_bitmap(int x, int y, int w, int h, const void* data,
                          uint32_t stride_bytes = 0) override;

  // SSD1306 requires page-aligned Y coordinates. We inject the rounding event.
  void on_lvgl_init(lv_display_t* disp) override;

 private:
  uint8_t* tx_buffer_ = nullptr;
  size_t tx_buffer_size_ = 0;
  uint8_t contrast_level_ = 0xff;
};

}  // namespace halpp