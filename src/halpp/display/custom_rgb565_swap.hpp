#pragma once

#include <cstdint>

// Optimization attribute: overrides the translation-unit -Os flag on GCC for
// performance-critical graphics loops.
// Clang and other compilers do not support gnu::optimize and ignore it or warn.
#if defined(__GNUC__) && !defined(__clang__)
#define HALPP_HOT_OPTIMIZE [[gnu::optimize("O3")]]
#else
#define HALPP_HOT_OPTIMIZE
#endif

namespace halpp::detail {

/// Swaps bytes of a single 16-bit RGB565 pixel in-place:
/// [0x1234] -> [0x3412]
[[nodiscard]] constexpr inline uint16_t swap_rgb565_pixel(uint16_t val) noexcept {
  return static_cast<uint16_t>((val >> 8) | (val << 8));
}

/// Swaps bytes in both 16-bit lanes of a packed 32-bit word (two pixels):
/// [0xAABBCCDD] -> [0xBBAADDCC]
///
/// On Xtensa (ESP32-S3): emitted inline via SSAI/SRC bit rotates without function calls.
/// On RISC-V (ESP32-C6): lowers to native bswap / bit-manipulation instructions.
/// On Host (x86/ARM): lowers to bswap + ror.
[[nodiscard]] inline uint32_t swap_rgb565_pair(uint32_t val) noexcept {
  const uint32_t swapped = __builtin_bswap32(val);
  return (swapped >> 16) | (swapped << 16);
}

}  // namespace halpp::detail

HALPP_HOT_OPTIMIZE
inline static void custom_rgb565_swap(void* __restrict buf, uint32_t buf_size_px) {
  uint16_t* __restrict buf16 = static_cast<uint16_t*>(buf);

  // 1. Enforce 32-bit alignment to avoid hardware fetch penalties
  if (reinterpret_cast<uintptr_t>(buf16) & 0x2) {
    buf16[0] = halpp::detail::swap_rgb565_pixel(buf16[0]);
    buf16++;
    buf_size_px--;
  }

  uint32_t* __restrict buf32 = reinterpret_cast<uint32_t*>(buf16);
  uint32_t u32_cnt = buf_size_px / 2;

  // 2. Main loop: unrolled 4 words (8 pixels / 16 bytes) per iteration.
  // On ESP32-S3, GCC schedules this into a hardware `loop` with pipelined
  // 32-bit loads and stores.
  uint32_t unrolled_cnt = u32_cnt / 4;
  for (uint32_t i = 0; i < unrolled_cnt; ++i) {
    uint32_t w0 = halpp::detail::swap_rgb565_pair(buf32[0]);
    uint32_t w1 = halpp::detail::swap_rgb565_pair(buf32[1]);
    uint32_t w2 = halpp::detail::swap_rgb565_pair(buf32[2]);
    uint32_t w3 = halpp::detail::swap_rgb565_pair(buf32[3]);
    buf32[0] = w0;
    buf32[1] = w1;
    buf32[2] = w2;
    buf32[3] = w3;
    buf32 += 4;
  }

  // Remainder words (up to 3 words = 6 pixels, fully unrolled by -O3)
  for (uint32_t i = 0; i < (u32_cnt & 3); ++i) {
    buf32[i] = halpp::detail::swap_rgb565_pair(buf32[i]);
  }

  // 3. Catch the trailing odd pixel
  if (buf_size_px & 0x1) {
    uint32_t last_idx = buf_size_px - 1;
    buf16[last_idx] = halpp::detail::swap_rgb565_pixel(buf16[last_idx]);
  }
}

#undef HALPP_HOT_OPTIMIZE
