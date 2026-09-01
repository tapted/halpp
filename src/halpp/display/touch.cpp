#if __has_include(<esp_lcd_touch.h>)
#include "halpp/display/touch.hpp"

#include <cstring>
#include <esp_log.h>
#include <hal/gpio_types.h>

#include "espbase/main_loop.hpp"
#include "halpp/config.hpp"
#include "halpp/display/display.hpp"
#include "halpp/i2c/i2c_master.hpp"

using halpp::config;

static constexpr const char TAG[] = "TOUCH";

namespace Config {
constexpr uint16_t MAX_X = config::Display::WIDTH - 1;
constexpr uint16_t MAX_Y = config::Display::HEIGHT - 1;
constexpr bool SWAP_XY = false;
constexpr bool MIRROR_X = false;
constexpr bool MIRROR_Y = false;
}  // namespace Config

namespace halpp::display {

Touch::~Touch() {
  reset();
}
EspResult<void> Touch::begin(TouchInitFn driver_init_fn, void (*on_screen_touched)()) {
  if (touch_handle_ || lv_indev_) return ESP_OK;
  this->on_screen_touched = on_screen_touched;

  ESP_LOGI(TAG, "Initializing Touch Driver...");

  esp_lcd_panel_io_i2c_config_t tp_io_config = {};
  memset(&tp_io_config, 0, sizeof(esp_lcd_panel_io_i2c_config_t));

  // 1. Use the dynamically passed I2C address
  tp_io_config.dev_addr = config::Touch::I2C_ADDRESS;
  tp_io_config.scl_speed_hz = config::I2CConfig::CLK_SPEED;
  tp_io_config.control_phase_bytes = 1;
  tp_io_config.dc_bit_offset = 0;
  tp_io_config.lcd_cmd_bits = 8;
  tp_io_config.flags.disable_control_phase = 1;

  if (esp_err_t err = esp_lcd_new_panel_io_i2c(I2CMaster::instance().get_bus_handle(),
                                               &tp_io_config, &io_handle_)) {
    return EspError(err).log(TAG, "Failed to create touch I2C IO");
  }

  esp_lcd_touch_config_t tp_config = {
      .x_max = Config::MAX_X,
      .y_max = Config::MAX_Y,
      .rst_gpio_num = GPIO_NUM_NC,
      .int_gpio_num = config::Touch::PIN_INTERRUPT,
      .levels = {.reset = 0, .interrupt = 0},
      .flags =
          {
              .swap_xy = Config::SWAP_XY,
              .mirror_x = Config::MIRROR_X,
              .mirror_y = Config::MIRROR_Y,
          },
      .process_coordinates = nullptr,

      // 2. Wire the ISR internally and pass 'this' context
      .interrupt_callback = internal_isr_cb,
      .user_data = this,
      .driver_data = nullptr,
  };

  if (esp_err_t err = driver_init_fn(io_handle_, &tp_config, &touch_handle_)) {
    reset();  // 3. Rollback the io_handle_ allocation
    return EspError(err).log(TAG, "Failed to execute touch driver init function");
  }

  lv_indev_ = lv_indev_create();
  if (!lv_indev_) {
    reset();  // 3. Rollback both IO and Touch handles
    return EspError(ESP_ERR_NO_MEM).log(TAG, "Failed to create LVGL input device");
  }

  lv_indev_set_type(lv_indev_, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_indev_, lvgl_read_cb);
  lv_indev_set_user_data(lv_indev_, this);
  lv_indev_set_mode(lv_indev_, LV_INDEV_MODE_EVENT);

  ESP_LOGI(TAG, "Touch initialization complete.");
  return ESP_OK;
}

void Touch::reset() {
  if (lv_indev_) {
    lv_indev_delete(lv_indev_);
    lv_indev_ = nullptr;
  }
  if (touch_handle_) {
    esp_lcd_touch_del(touch_handle_);
    touch_handle_ = nullptr;
  }
  if (io_handle_) {
    esp_lcd_panel_io_del(io_handle_);
    io_handle_ = nullptr;
  }
}

void Touch::maybe_indev_read() {
  task_pending_.store(false, std::memory_order_release);
  if (lv_indev_ && (irq_fired_ || is_touching_)) {
    halpp::Display::Guard lock;
    lv_indev_read(lv_indev_);
    if (on_screen_touched) on_screen_touched();
  }
}

void Touch::lvgl_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  Touch* touch = static_cast<Touch*>(lv_indev_get_user_data(indev));

  // 1. THE STICKY GATE
  if (touch->irq_fired_ || touch->is_touching_) {
    esp_lcd_touch_read_data(touch->touch_handle_);
    touch->irq_fired_ = false;
  }

  // 2. Read the driver's cache
  esp_lcd_touch_point_data_t touch_data[1];
  uint8_t cnt = 0;
  esp_err_t err = esp_lcd_touch_get_data(touch->touch_handle_, touch_data, &cnt, 1);
  const bool is_currently_touching = (err == ESP_OK && cnt > 0);

  // Keep the I2C gate open for the next LVGL tick if touching
  touch->is_touching_ = is_currently_touching;

  // 3. Screen Sleep & Ghost Click Protection
  if (is_currently_touching && (touch->ignore_until_release_ || !touch->is_enabled_)) {
    touch->ignore_until_release_ = true;
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  touch->ignore_until_release_ = false;

  // 4. Pass data to LVGL
  if (is_currently_touching) {
    data->point.x = touch_data[0].x;
    data->point.y = touch_data[0].y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

IRAM_ATTR void Touch::internal_isr_cb(esp_lcd_touch_handle_t tp) {
  Touch* self = static_cast<Touch*>(tp->config.user_data);
  if (!self) return;

  // 1. Set the internal flag to open the LVGL read gate
  self->irq_fired_ = true;
  if constexpr (config::lvgl::USE_MAIN_LOOP) {
    if (self->task_pending_.exchange(true, std::memory_order_acq_rel)) return;  // Already scheduled
    main_loop.push<&Touch::maybe_indev_read>(self);
  } else {
    if (self->on_screen_touched) self->on_screen_touched();
  }
}
}  // namespace halpp::display

#else
#pragma message("Install espressif/esp_lcd_touch to use Touch module")
#endif