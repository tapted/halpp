#include "halpp/segmented/i2c_7seg.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sys/time.h>

#include "halpp/i2c/i2c_master.hpp"

namespace HAL {

static const char* TAG = "I2C7Seg";

// ASCII to 7-segment bitmask table
static constexpr std::array<uint8_t, 96> kSevenSegFontTable = {
    0b00000000, 0b10000110, 0b00100010, 0b01111110, 0b01101101, 0b11010010, 0b01000110, 0b00100000,
    0b00101001, 0b00001011, 0b00100001, 0b01110000, 0b00010000, 0b01000000, 0b10000000, 0b01010010,
    0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111,
    0b01111111, 0b01101111, 0b00001001, 0b00001101, 0b01100001, 0b01001000, 0b01000011, 0b11010011,
    0b01011111, 0b01110111, 0b01111100, 0b00111001, 0b01011110, 0b01111001, 0b01110001, 0b00111101,
    0b01110110, 0b00110000, 0b00011110, 0b01110101, 0b00111000, 0b00010101, 0b00110111, 0b00111111,
    0b01110011, 0b01101011, 0b00110011, 0b01101101, 0b01111000, 0b00111110, 0b00111110, 0b00101010,
    0b01110110, 0b01101110, 0b01011011, 0b00111001, 0b01100100, 0b00001111, 0b00100011, 0b00001000,
    0b00000010, 0b01011111, 0b01111100, 0b01011000, 0b01011110, 0b01111011, 0b01110001, 0b01101111,
    0b01110100, 0b00010000, 0b00001100, 0b01110101, 0b00110000, 0b00010100, 0b01010100, 0b01011100,
    0b01110011, 0b01100111, 0b01010000, 0b01101101, 0b01111000, 0b00011100, 0b00011100, 0b00010100,
    0b01110110, 0b01101110, 0b01011011, 0b01000110, 0b00110000, 0b01110000, 0b00000001, 0b00000000,
};

EspResult<void> I2C7Seg::init_default(uint8_t i2c_address) {
  auto& inst = default_instance();
  if (inst.is_initialized()) return ESP_OK;

  // The rvalue check neatly consumes the temporary EspResult and moves the handle
  if (EspError err =
          EspError::check(I2CMaster::instance().add_device(i2c_address), &inst.i2c_dev_)) {
    return err.log(TAG, "Failed to add default HT16K33 to I2C bus");
  }

  return inst.begin();
}

EspResult<void> I2C7Seg::deinit_default() {
  auto& inst = default_instance();
  if (!inst.is_initialized()) return ESP_OK;
  return inst.i2c_dev_.reset();
}

EspResult<void> I2C7Seg::begin() {
  if (!i2c_dev_) return ESP_ERR_INVALID_STATE;

  // 0x21 turns on the internal oscillator
  if (EspError err = i2c_dev_.tx({0x21})) return err;

  clear();
  if (EspError err = write_display()) return err;

  if (EspError err = set_blink_rate(BlinkRate::Off)) return err;

  return set_brightness(15);
}

EspResult<void> I2C7Seg::set_brightness(uint8_t b) {
  b = std::min<uint8_t>(b, 15);
  uint8_t cmd = 0xE0 | b;
  return i2c_dev_.tx({cmd});
}

EspResult<void> I2C7Seg::set_blink_rate(BlinkRate rate) {
  uint8_t cmd = 0x80 | 0x01 | (static_cast<uint8_t>(rate) << 1);
  return i2c_dev_.tx({cmd});
}

EspResult<void> I2C7Seg::set_display_state(bool on) {
  uint8_t cmd = 0x80 | (on ? 0x01 : 0x00);
  return i2c_dev_.tx({cmd});
}

EspResult<void> I2C7Seg::write_display() {
  uint8_t buffer[17] = {0};
  for (size_t i = 0; i < 8; i++) {
    buffer[1 + 2 * i] = display_buffer_[i] & 0xFF;
    buffer[2 + 2 * i] = (display_buffer_[i] >> 8) & 0xFF;
  }
  return i2c_dev_.tx(buffer);
}

void I2C7Seg::clear() {
  display_buffer_.fill(0);
}

void I2C7Seg::write_digit_raw(uint8_t digit, uint8_t bitmask) {
  if (digit > 4) return;
  display_buffer_[digit] = bitmask;
}

void I2C7Seg::write_digit_ascii(uint8_t digit, char c, bool dot) {
  if (digit > 4) return;

  uint8_t font = 0;
  if (c >= ' ' && c <= '~') {
    font = kSevenSegFontTable[c - ' '];
  }

  write_digit_raw(digit, font | (dot ? (1 << 7) : 0));
}

void I2C7Seg::draw_colon(bool state) {
  display_buffer_[2] = state ? 0x2 : 0;
}

void I2C7Seg::print_error() {
  for (uint8_t i = 0; i < 5; ++i) {
    write_digit_raw(i, (i == 2 ? 0x00 : 0x40));
  }
}

void I2C7Seg::print(std::string_view str) {
  uint16_t saved_dots = display_buffer_[2];
  clear();
  display_buffer_[2] = saved_dots;

  int8_t digit_idx = 4;

  for (int i = static_cast<int>(str.length()) - 1; i >= 0; --i) {
    if (digit_idx < 0) break;
    if (digit_idx == 2) digit_idx--;

    bool dot = false;
    char c = str[i];

    if ((c == '.' || c == ',') && i > 0) {
      dot = true;
      i--;
      c = str[i];
    } else if (c == '.' || c == ',') {
      continue;
    }

    write_digit_ascii(digit_idx, c, dot);
    digit_idx--;
  }
}

void I2C7Seg::print_number(int n) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", std::abs(n));
  print(buf);
}

void I2C7Seg::print_float(double n, uint8_t frac_digits) {
  char buf[32];
  // The "%.*f" specifier allows us to pass precision dynamically via an argument
  snprintf(buf, sizeof(buf), "%.*f", frac_digits, n);
  print(buf);
}

uint32_t I2C7Seg::show_time() {
  draw_colon(true);
  // Get high-precision system time
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  // Format and render to the display
  time_t now = tv.tv_sec;
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  print_number(timeinfo.tm_hour * 100 + timeinfo.tm_min);
  write_display().log_error(TAG, "Failed to update time display");

  ESP_LOGI(TAG, "Current time: %02d:%02d:%02d.%03ld", timeinfo.tm_hour, timeinfo.tm_min,
           timeinfo.tm_sec, tv.tv_usec / 1000);
  
  // Safety Margin (Target 50ms past the second)
  constexpr uint32_t TARGET_OFFSET_MS = 50;
  // Calculate exactly how many milliseconds we are into the current minute
  // tm_sec is 0-59. tv_usec is 0-999999.
  uint32_t ms_passed_in_minute = (timeinfo.tm_sec * 1000) + (tv.tv_usec / 1000);

  uint32_t delay_ms;

  // Calculate delay to the next target offset
  if (ms_passed_in_minute < TARGET_OFFSET_MS) {
    // We woke up fractions of a millisecond early (e.g. at 00:00.010). 
    // Wait the remaining 40ms to clear the safety margin.
    delay_ms = TARGET_OFFSET_MS - ms_passed_in_minute;
  } else {
    // We are safely past the offset. Wait until the NEXT minute's offset.
    // A full minute is 60,000 milliseconds.
    delay_ms = 60000 - ms_passed_in_minute + TARGET_OFFSET_MS;
  }

  // Fallback safeguard: prevent 0ms spins
  if (delay_ms == 0) delay_ms = 60000;

  return delay_ms;
}

}  // namespace HAL