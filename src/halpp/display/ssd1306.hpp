/**
 * @file ssd1306.hpp
 * @brief Modular, RAII wrapper for ESP-IDF esp_lcd SSD1306 OLED displays.
 * @details Decouples the physical bus (I2C/SPI) from the display driver
 * by operating purely on esp_lcd_panel_io_handle_t.
 */

#pragma once

#include <cstdint>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <optional>

#include "espbase/esp_result.hpp"

namespace HAL {

class Ssd1306 {
 public:
  struct Config {
    uint16_t width = 128;
    uint16_t height = 64;
    esp_lcd_panel_io_handle_t io_handle = nullptr;  // Abstract physical bus
    bool owns_io_handle = false;                    // Determines if ~Ssd1306 deletes the IO handle
  };

  constexpr Ssd1306() = default;
  explicit Ssd1306(Config config) : config_(config) {}
  ~Ssd1306() { reset(); }

  // Strict ownership semantics. Movable, never copyable.
  Ssd1306(const Ssd1306&) = delete;
  Ssd1306& operator=(const Ssd1306&) = delete;
  Ssd1306(Ssd1306&& other) noexcept;
  Ssd1306& operator=(Ssd1306&& other) noexcept;

  // --- Single-Device Default Pattern ---
  static Ssd1306& default_instance() {
    static std::optional<Ssd1306> inst;
    if (!inst) inst.emplace();
    return *inst;
  }

  // Factory convenience method that strictly configures an I2C IO layer
  static EspResult<void> init_default_i2c(uint8_t i2c_address = 0x3C, uint16_t width = 128,
                                          uint16_t height = 64);

  static EspResult<void> deinit_default() { return default_instance().reset(); }

  // --- Display Operations ---
  bool is_initialized() const { return panel_handle_ != nullptr; }

  // Initializes the OLED chip (wakes up, configures RAM, turns on display)
  EspResult<void> begin();

  // Tears down the esp_lcd handles safely
  EspResult<void> reset();

  EspResult<void> draw_bitmap(int x_start, int y_start, int width, int height,
                              const uint8_t* color_data);
  EspResult<void> clear();

  EspResult<void> set_display_state(bool on);
  EspResult<void> invert(bool inverted);

 private:
  Config config_;
  esp_lcd_panel_handle_t panel_handle_ = nullptr;
};

}  // namespace HAL