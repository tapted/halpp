#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "espbase/esp_result.hpp"
#include "halpp/i2c/i2c_device.hpp"


namespace HAL {

class I2C7Seg {
 public:
  enum class BlinkRate : uint8_t { Off = 0, Hz_2 = 1, Hz_1 = 2, Half_Hz = 3 };

  // --- Pattern 1: Multi-Display (Explicit Ownership) ---
  // The default constructor allows for an empty handle, used by the singleton.
  explicit I2C7Seg(I2CDevice device = I2CDevice{}) : i2c_dev_(std::move(device)) {}

  // Executes hardware initialization over I2C. MUST be called after construction.
  EspResult<void> begin();

  // --- Pattern 2: Single-Display Default ---
  // Optimizes for the common case of a single display connected to the bus.
  static I2C7Seg& default_instance() {
    static I2C7Seg inst;
    return inst;
  }

  // Initializes the default instance and attaches it to the bus
  static EspResult<void> init_default(uint8_t i2c_address = 0x70);

  // Releases the device handle for the default instance
  static EspResult<void> deinit_default();

  // --- Display Operations ---
  bool is_initialized() const { return !!i2c_dev_; }

  EspResult<void> set_brightness(uint8_t b);
  EspResult<void> set_blink_rate(BlinkRate rate);
  EspResult<void> set_display_state(bool on);

  EspResult<void> write_display();
  void clear();

  void write_digit_raw(uint8_t digit, uint8_t bitmask);
  void write_digit_ascii(uint8_t digit, char c, bool dot = false);
  void draw_colon(bool state);
  void print_error();

  void print(std::string_view str);
  void print_number(int n);
  void print_float(double n, uint8_t frac_digits = 2);

 private:
  I2CDevice i2c_dev_;
  std::array<uint16_t, 8> display_buffer_{0};
};

}  // namespace HAL