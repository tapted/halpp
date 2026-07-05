/**
 * @file display.hpp
 * @brief Universal base class for esp_lcd compatible displays.
 */

#pragma once

#include <cstdint>
#include <esp_lcd_types.h>
#include <mutex>

#include "espbase/esp_result.hpp"

struct _lv_display_t;
typedef struct _lv_display_t lv_display_t;

namespace HAL {

class Display {
 private:
  struct DisplayLock;

 public:
  // RAII-style lock for thread-safe LVGL access. Locks the internal mutex on construction. Unlocks
  // and notifies the LVGL task to redraw on destruction.
  struct Guard {
    std::lock_guard<DisplayLock> lock{mutex};
  };

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

  bool is_initialized() const { return panel_handle_ != nullptr; }

  EspResult<void> init_lvgl();
  lv_display_t* get_lv_display() const { return lv_display_; }

  uint16_t width() const { return config_.width; }
  uint16_t height() const { return config_.height; }

  // --- Universal Drawing Primitives ---
  EspResult<void> reset();
  EspResult<void> set_display_state(bool on);
  EspResult<void> invert(bool inverted);
  EspResult<void> swap_xy(bool swap);

  // Fills a rectangle safely using a fixed-size DMA chunking buffer
  EspResult<void> fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint32_t color);
  EspResult<void> clear();

  // Virtualized so subclasses can intercept and transpose raw data (like SSD1306)
  virtual EspResult<void> draw_bitmap(int x_start, int y_start, int width, int height,
                                      const void* color_data, uint32_t stride_bytes = 0);

  // New hook for LVGL indexed formats (cleanly separates the ARGB8888 palette from the pixels)
  virtual EspResult<void> draw_indexed_bitmap(int x_start, int y_start, int width, int height,
                                              const void* pixel_data, const void* palette,
                                              uint32_t stride_bytes = 0);

  // Optional hook for subclasses to inject hardware-specific LVGL events (like coordinate rounding)
  virtual void on_lvgl_init(lv_display_t* disp);

 protected:
  Config config_;
  esp_lcd_panel_handle_t panel_handle_ = nullptr;
  lv_display_t* lv_display_ = nullptr;

  static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                  esp_lcd_panel_io_event_data_t* edata, void* user_ctx);

 private:
  // Locks an internal mutex and notifies LVGL to redraw on unlock.
  struct DisplayLock {
   private:
    friend class std::lock_guard<DisplayLock>;
    void lock();
    void unlock();
  };

  static DisplayLock mutex;

  // Non-copyable, non-movable, so we can pass `this` as user_ctx.
  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;
  Display(Display&& other) = delete;
  Display& operator=(Display&& other) = delete;
};

}  // namespace HAL