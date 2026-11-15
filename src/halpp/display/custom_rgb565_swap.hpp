#include <cstdint>

inline static void custom_rgb565_swap(void* __restrict buf, uint32_t buf_size_px) {
  uint16_t* __restrict buf16 = static_cast<uint16_t*>(buf);

  // 1. Enforce 32-bit alignment to avoid hardware fetch penalties
  if (reinterpret_cast<uintptr_t>(buf16) & 0x2) {
    uint16_t val = buf16[0];
    buf16[0] = (val >> 8) | (val << 8);
    buf16++;
    buf_size_px--;
  }

  uint32_t* __restrict buf32 = reinterpret_cast<uint32_t*>(buf16);
  uint32_t u32_cnt = buf_size_px / 2;

  // 2. A clean loop allows the compiler to perfectly pipeline the ALU operations
  for (uint32_t i = 0; i < u32_cnt; ++i) {
    uint32_t val = buf32[i];
    buf32[i] = ((val & 0xff00ff00) >> 8) | ((val & 0x00ff00ff) << 8);
  }

  // 3. Catch the trailing odd pixel
  if (buf_size_px & 0x1) {
    uint32_t last_idx = buf_size_px - 1;
    uint16_t val = buf16[last_idx];
    buf16[last_idx] = (val >> 8) | (val << 8);
  }
}
