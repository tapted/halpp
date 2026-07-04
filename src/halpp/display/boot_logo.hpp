/**
 * @file boot_logo.hpp
 * @brief Compile-time generated assets for the SSD1306 display.
 */

#pragma once

#include <array>
#include <cstdint>

#include "halpp/config.hpp"


namespace HAL::Assets {

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
      if (draw != config::Display::INVERT_COLORS) {
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

constexpr auto BOOT_LOGO = generate_boot_logo<128, 64>();

}  // namespace HAL::Assets