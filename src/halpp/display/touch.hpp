#pragma once

#include <esp_lcd_panel_io.h>
#include <esp_lcd_touch.h>
#include <indev/lv_indev.h>
#include <misc/lv_types.h>

#include "espbase/esp_result.hpp"

typedef struct esp_lcd_panel_io_t *esp_lcd_panel_io_handle_t;

namespace halpp::display {

// Generic function pointer matching all esp_lcd_touch_new_i2c_* drivers
using TouchInitFn = esp_err_t (*)(const esp_lcd_panel_io_handle_t io,
                                  const esp_lcd_touch_config_t* config,
                                  esp_lcd_touch_handle_t* out_touch);

class Touch {
 public:
  constexpr Touch() = default;
  ~Touch();

  Touch(const Touch&) = delete;
  Touch& operator=(const Touch&) = delete;

  // Initializes the I2C IO, Touch Controller, and LVGL Input Device.
  // `on_screen_touched` is called from the main loop when a touch event is detected OR if 
  // config::lvgl::USE_MAIN_LOOP is false, it is called directly from the ISR context (without 
  // asking lvgl to read the input device).
  EspResult<> begin(TouchInitFn driver_init_fn, void (*on_screen_touched)());
  void reset();

  // Forces LVGL to read the touch data if an event is pending
  void maybe_indev_read();

  // Toggle this when the screen goes to sleep so inputs are ignored
  void set_enabled(bool enabled) { is_enabled_ = enabled; }

 private:
  esp_lcd_panel_io_handle_t io_handle_ = nullptr;
  esp_lcd_touch_handle_t touch_handle_ = nullptr;
  lv_indev_t* lv_indev_ = nullptr;
  void (*on_screen_touched)() = nullptr;

  volatile bool irq_fired_ = false;
  bool is_touching_ = false;
  bool is_enabled_ = true;
  bool ignore_until_release_ = false;  // Prevents phantom clicks when waking up

  static void lvgl_read_cb(lv_indev_t* indev, lv_indev_data_t* data);
  static void internal_isr_cb(esp_lcd_touch_handle_t tp);
};

}  // namespace halpp::display