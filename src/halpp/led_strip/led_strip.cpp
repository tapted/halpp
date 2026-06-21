#include "halpp/led_strip/led_strip.hpp"

#include <led_strip.h>
#include <led_strip_rmt.h>

namespace HAL {

LedStrip::LedStrip(LedStrip&& other) noexcept {
  handle_ = other.handle_;
  other.handle_ = nullptr;
}

LedStrip& LedStrip::operator=(LedStrip&& other) noexcept {
  if (this != &other) {
    reset();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

EspResult<LedStrip> LedStrip::create_rmt(const RmtConfig& config) {
  led_strip_config_t strip_config = {};
  strip_config.strip_gpio_num = config.gpio_num;
  strip_config.max_leds = config.max_leds;
  strip_config.led_model = config.model;
  strip_config.color_component_format = config.color_fmt;
  strip_config.flags.invert_out = false;

  led_strip_rmt_config_t rmt_config = {};
  rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
  rmt_config.resolution_hz = config.resolution_hz;
  rmt_config.mem_block_symbols = 0;
  rmt_config.flags.with_dma = config.with_dma;

  led_strip_handle_t handle = nullptr;
  if (esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &handle)) {
    return EspResult<LedStrip>::fail(err);
  }

  // Ensure LEDs are off by default upon initialization
  led_strip_clear(handle);

  return EspResult<LedStrip>::ok(LedStrip(handle));
}

EspResult<void> LedStrip::reset() {
  if (handle_) {
    // Optionally clear before deleting so they don't stay lit
    led_strip_clear(handle_);
    esp_err_t err = led_strip_del(handle_);
    handle_ = nullptr;
    return err;
  }
  return ESP_OK;
}

EspResult<void> LedStrip::set_pixel(uint32_t index, uint32_t r, uint32_t g, uint32_t b) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return led_strip_set_pixel(handle_, index, r, g, b);
}

EspResult<void> LedStrip::set_pixel_rgbw(uint32_t index, uint32_t r, uint32_t g, uint32_t b,
                                         uint32_t w) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return led_strip_set_pixel_rgbw(handle_, index, r, g, b, w);
}

EspResult<void> LedStrip::set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t sat, uint8_t val) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return led_strip_set_pixel_hsv(handle_, index, hue, sat, val);
}

EspResult<void> LedStrip::set_pixel_hsv_16(uint32_t index, uint16_t hue, uint16_t sat,
                                           uint16_t val) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return led_strip_set_pixel_hsv_16(handle_, index, hue, sat, val);
}

EspResult<void> LedStrip::refresh() {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return led_strip_refresh(handle_);
}

EspResult<void> LedStrip::clear() {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return led_strip_clear(handle_);
}

}  // namespace HAL