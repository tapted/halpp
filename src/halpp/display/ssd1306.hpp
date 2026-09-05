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

  // --- Hardware Quirks Overrides ---
  // SSD1306 requires vertical-page formatting. We intercept and transpose.
  EspResult<void> draw_bitmap(int x, int y, int w, int h, const void* data,
                              uint32_t stride_bytes = 0) override;

  // SSD1306 requires page-aligned Y coordinates. We inject the rounding event.
  void on_lvgl_init(lv_display_t* disp) override;

 private:
  uint8_t* tx_buffer_ = nullptr;
  size_t tx_buffer_size_ = 0;
};

}  // namespace halpp