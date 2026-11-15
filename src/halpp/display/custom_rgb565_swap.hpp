#include <cstdint>

// [[gnu::optimize("O2")]] overrides the translation-unit's -Os flag for this
// function.
//
// At -Os, GCC pattern-matches the byte-swap-within-16-bit-lanes expression and
// lowers it to `call8 __bswapsi2` (a libgcc helper), because Xtensa has no
// native bswap instruction and a function call is smaller than the equivalent
// inline shift sequence.  That saves a handful of bytes of .text but costs a
// real call+return on every loop iteration — a poor trade for a hot pixel-DMA
// callback.
//
// At -O2 the same pattern is instead emitted inline as an extui/slli/or
// sequence, eliminating the per-iteration call overhead.  The loop may also be
// unrolled, which can further hide load/store latency.
//
// Making the transform explicit with __builtin_bswap32 + __builtin_rotateright32
// (mathematically identical to the bitmask form: bswap32(x) ror 16 == swap
// bytes within each 16-bit lane) communicates intent clearly and gives GCC's
// pattern-matcher the clearest possible signal.
//
// --- ESP32-S3 vector (PIE) path ---
// The ESP32-S3's Processor Instruction Extensions (PIE) vector unit can process
// 128 bits per cycle, which would let the loop body handle eight 16-bit pixels
// per iteration instead of two.  However, GCC's auto-vectoriser does not target
// the PIE ISA — there is no __attribute__((target(...))) or pragma that enables
// it.  Exploiting it requires manual TIE intrinsics from
// <xtensa/tie/esp_nn.h> / <xtensa/tie/esp_vector_alu.h>.  That is a future
// optimisation step once the scalar path is confirmed optimal.
[[gnu::optimize("O2")]]
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

  // 2. Equivalent to swapping bytes within each 16-bit lane:
  //      bswap32(AABBCCDD) = DDCCBBAA
  //      ror16(DDCCBBAA)   = BBAADDCC  ✓ (each pixel byte-swapped)
  //    __builtin_bswap32 makes the intent explicit; the rotate-right-16 is
  //    written as (x >> 16) | (x << 16) which GCC pattern-matches to a single
  //    rotate and emits as an inline shift sequence at -O2 without calling
  //    __bswapsi2.  (__builtin_rotateright32 is Clang-only, not available in
  //    xtensa-esp32s3-elf-g++.)
  for (uint32_t i = 0; i < u32_cnt; ++i) {
    const uint32_t swapped = __builtin_bswap32(buf32[i]);
    buf32[i] = (swapped >> 16) | (swapped << 16);
  }

  // 3. Catch the trailing odd pixel
  if (buf_size_px & 0x1) {
    uint32_t last_idx = buf_size_px - 1;
    uint16_t val = buf16[last_idx];
    buf16[last_idx] = (val >> 8) | (val << 8);
  }
}
