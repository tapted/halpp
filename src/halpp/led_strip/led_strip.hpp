/**
 * @file led_strip.hpp
 * @brief RAII wrapper for ESP-IDF LED Strip Driver
 */

#pragma once

#include <cstdint>
#include <soc/gpio_num.h>
#include <led_strip_types.h>

#include "espbase/esp_result.hpp"

namespace HAL {

// Configuration struct with defaults optimized for WS2812
struct RmtConfig {
  gpio_num_t gpio_num;
  uint32_t max_leds = 1;
  led_model_t model = LED_MODEL_WS2812;
  led_color_component_format_t color_fmt = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
  uint32_t resolution_hz = 10 * 1000 * 1000;  // 10MHz resolution default
  bool with_dma = false;
};

class LedStrip {
 public:
  constexpr LedStrip() = default;
  ~LedStrip() { reset(); }

  // Can be moved. Not copied.
  LedStrip(const LedStrip&) = delete;
  LedStrip& operator=(const LedStrip&) = delete;
  LedStrip(LedStrip&& other) noexcept;
  LedStrip& operator=(LedStrip&& other) noexcept;

  explicit operator bool() const { return handle_ != nullptr; }
  bool operator!() const { return handle_ == nullptr; }

  static EspResult<LedStrip> create_rmt(const RmtConfig& config);

  EspResult<void> reset();

  // --- Pixel Operations ---
  EspResult<void> set_pixel(uint32_t index, uint32_t r, uint32_t g, uint32_t b);
  EspResult<void> set_pixel_rgbw(uint32_t index, uint32_t r, uint32_t g, uint32_t b, uint32_t w);
  EspResult<void> set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t sat, uint8_t val);
  EspResult<void> set_pixel_hsv_16(uint32_t index, uint16_t hue, uint16_t sat, uint16_t val);

  EspResult<void> refresh();
  EspResult<void> clear();

 private:
  led_strip_handle_t handle_ = nullptr;

  explicit LedStrip(led_strip_handle_t handle) : handle_(handle) {}
};

}  // namespace HAL