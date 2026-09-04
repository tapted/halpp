/**
 * @file boot_logo.hpp
 * @brief Compile-time generated assets for the SSD1306 display.
 */

#pragma once

#include <array>
#include <cstdint>

#include "halpp/config.hpp"

namespace halpp::Assets {

/**
 * @brief Generates a SSD1306-formatted bitmap of a tech/radar spinner.
 * @details Evaluated entirely at compile time. Maps XY coordinates to the
 * specific 8-bit page layout required by the SSD1306 hardware.
 * @tparam Width Width of the display in pixels.
 * @tparam Height Height of the display in pixels (must be a multiple of 8).
 */
template <uint16_t Width = 128, uint16_t Height = 64>
constexpr std::array<uint8_t, (Width * Height) / 8> generate_boot_logo() {
  // SSD1306 memory is divided into 8-pixel high pages.
  // If height isn't a multiple of 8, our memory mapping math will overflow.
  static_assert(Height % 8 == 0, "Display height must be a multiple of 8 for SSD1306 paging.");

  std::array<uint8_t, (Width * Height) / 8> buffer = {0};

  const int center_x = Width / 2;
  const int center_y = Height / 2;

  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      // Center the logo dynamically
      int dx = x - center_x;
      int dy = y - center_y;
      int r2 = dx * dx + dy * dy;
      bool draw = false;

      // 2. Middle "spinning" track (Gap in the top-right quadrant)
      if (r2 >= 18 * 18 && r2 <= 21 * 21) {
        if (!(dx > 0 && dy < 0)) {  // Skip top-right
          draw = true;
        }
      }

      // 3. Inner "spinning" track (Gap in the bottom-left quadrant)
      if (r2 >= 10 * 10 && r2 <= 13 * 13) {
        if (!(dx < 0 && dy > 0)) {  // Skip bottom-left
          draw = true;
        }
      }

      // 4. Solid center core
      if (r2 <= 4 * 4) {
        draw = true;
      }

      // If the pixel is part of the logo, map it to the SSD1306 page memory layout
      if (draw != halpp::config::Display::INVERT_COLORS) {
        // SSD1306 splits pixels of height into 8 "pages" of 8 bits each.
        // LSB is top, MSB is bottom for each byte.
        int page = y / 8;
        int bit = y % 8;
        // Map to a 1D array using the dynamic Width
        buffer[x + page * Width] |= (1 << bit);
      }
    }
  }
  return buffer;
}
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

template <uint16_t Size = 128>
constexpr std::array<uint16_t, Size * Size> generate_color_logo() {
  std::array<uint16_t, Size * Size> buffer = {0};
  const int center = Size / 2;

  for (int y = 0; y < Size; ++y) {
    for (int x = 0; x < Size; ++x) {
      int dx = x - center;
      int dy = y - center;
      int r2 = dx * dx + dy * dy;
      bool draw = false;

      if (r2 >= 45 * 45 && r2 <= 52 * 52 && !(dx > 0 && dy < 0)) draw = true;
      if (r2 >= 25 * 25 && r2 <= 32 * 32 && !(dx < 0 && dy > 0)) draw = true;
      if (r2 <= 10 * 10) draw = true;

      if (draw) {
        uint8_t r = (x * 255) / Size;
        uint8_t b = (y * 255) / Size;
        uint8_t g = 255 - ((r + b) / 2);

        // Output native Little Endian color
        buffer[y * Size + x] = rgb565(r, g, b);
      } else {
        // Deep space background
        buffer[y * Size + x] = rgb565(5, 5, 15);
      }
    }
  }
  return buffer;
}

constexpr uint16_t MONOCHROME_LOGO_WIDTH = 128;
constexpr uint16_t MONOCHROME_LOGO_HEIGHT = 64;
constexpr auto MONOCHROME_BOOT_LOGO =
    generate_boot_logo<MONOCHROME_LOGO_WIDTH, MONOCHROME_LOGO_HEIGHT>();

constexpr uint16_t COLOR_LOGO_SIZE = 128;
constexpr uint16_t COLOR_LOGO_BG_COLOR = rgb565(5, 5, 15);
constexpr auto COLOR_LOGO = generate_color_logo<COLOR_LOGO_SIZE>();

}  // namespace halpp::Assets