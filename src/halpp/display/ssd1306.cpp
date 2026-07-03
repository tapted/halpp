#include "halpp/display/ssd1306.hpp"

#include <cstring>
#include <esp_log.h>

#include "halpp/i2c/i2c_master.hpp"
#include "halpp/display/boot_logo.hpp"

namespace HAL {

static const char* TAG = "Ssd1306";

Ssd1306::Ssd1306(Ssd1306&& other) noexcept {
  config_ = other.config_;
  panel_handle_ = other.panel_handle_;
  other.panel_handle_ = nullptr;
  other.config_.io_handle = nullptr;
}

Ssd1306& Ssd1306::operator=(Ssd1306&& other) noexcept {
  if (this != &other) {
    reset();
    config_ = other.config_;
    panel_handle_ = other.panel_handle_;
    other.panel_handle_ = nullptr;
    other.config_.io_handle = nullptr;
  }
  return *this;
}

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
      .on_color_trans_done = nullptr,
      .user_ctx = nullptr,
      .flags =
          {
              .dc_low_on_data = 0,
              .disable_control_phase = 0,
          },
  };

  esp_lcd_panel_io_handle_t io_handle = nullptr;

  // 2. Link the display IO to halpp's modern I2C Master bus
  if (EspError err = EspError::check(esp_lcd_new_panel_io_i2c(
          I2CMaster::instance().get_bus_handle(), &io_config, &io_handle))) {
    return err.log(TAG, "Failed to create I2C IO handle");
  }

  // 3. Inject the IO handle into the class configuration
  inst.config_ = Config{
      .width = width,
      .height = height,
      .io_handle = io_handle,
      .owns_io_handle =
          true  // The Ssd1306 class will cleanly delete the I2C IO handle on destruction
  };

  return inst.begin();
}

EspResult<void> Ssd1306::begin() {
  if (!config_.io_handle) return ESP_ERR_INVALID_STATE;
  if (is_initialized()) return ESP_OK;

  // Matches the exact struct layout to satisfy C++20 designated initializers
  esp_lcd_panel_dev_config_t panel_config = {
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // Ignored by monochrome SSD1306
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG,      // Ignored by monochrome SSD1306
      .bits_per_pixel = 1,
      .reset_gpio_num = static_cast<gpio_num_t>(
          -1),  // I2C modules typically don't have a hardware reset pin wired
      .vendor_config = nullptr,
      .flags =
          {
              .reset_active_high = 0,
          },
  };

  // 1. Create the panel driver from the IO handle
  if (EspError err = EspError::check(
          esp_lcd_new_panel_ssd1306(config_.io_handle, &panel_config, &panel_handle_))) {
    return err.log(TAG, "Failed to create SSD1306 panel handle");
  }

  // 2. Initialize the hardware chip
  if (EspError err = esp_lcd_panel_reset(panel_handle_)) return err;
  if (EspError err = esp_lcd_panel_init(panel_handle_)) return err;

  if (config_.width != 128 || config_.height != 64) {
    clear();  // Ensure RAM doesn't show static on boot
    ESP_LOGW(TAG, "Non-standard SSD1306 dimensions (%dx%d) may not be supported", config_.width,
             config_.height);
  } else {
    draw_bitmap(0, 0, config_.width, config_.height, Assets::BOOT_LOGO.data());
  }

  if (EspError err = esp_lcd_panel_disp_on_off(panel_handle_, true)) return err;

  ESP_LOGI(TAG, "SSD1306 Initialized (%dx%d)", config_.width, config_.height);
  return ESP_OK;
}

EspResult<void> Ssd1306::reset() {
  esp_err_t final_err = ESP_OK;

  if (panel_handle_) {
    final_err = esp_lcd_panel_del(panel_handle_);
    panel_handle_ = nullptr;
  }

  // If we created the IO handle internally (like in init_default_i2c), we must clean it up
  if (config_.owns_io_handle && config_.io_handle) {
    esp_err_t io_err = esp_lcd_panel_io_del(config_.io_handle);
    if (final_err == ESP_OK) final_err = io_err;
    config_.io_handle = nullptr;
  }

  return final_err;
}

EspResult<void> Ssd1306::draw_bitmap(int x_start, int y_start, int width, int height,
                                     const uint8_t* color_data) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  // esp_lcd expects coordinates as bounding boxes (x_start, y_start, x_end, y_end)
  return esp_lcd_panel_draw_bitmap(panel_handle_, x_start, y_start, x_start + width,
                                   y_start + height, color_data);
}

EspResult<void> Ssd1306::clear() {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;

  // The SSD1306 is 1-bit per pixel. A 128x64 display requires exactly 1024 bytes.
  // Allocating 1KB on the stack avoids heap fragmentation and is perfectly safe
  // for the ESP32 (FreeRTOS tasks default to 4-8KB stacks).
  const size_t buffer_size = (config_.width * config_.height) / 8;
  if (buffer_size > 1024) return ESP_ERR_NO_MEM;  // Guard against crazy user configs

  uint8_t zero_buffer[1024] = {0};
  return esp_lcd_panel_draw_bitmap(panel_handle_, 0, 0, config_.width, config_.height, zero_buffer);
}

EspResult<void> Ssd1306::set_display_state(bool on) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_disp_on_off(panel_handle_, on);
}

EspResult<void> Ssd1306::invert(bool inverted) {
  if (!panel_handle_) return ESP_ERR_INVALID_STATE;
  return esp_lcd_panel_invert_color(panel_handle_, inverted);
}

}  // namespace HAL