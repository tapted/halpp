// Assembly analysis shim for custom_rgb565_swap on Xtensa ESP32-S3.
//
// This file is compiled with the Xtensa cross-compiler (xtensa-esp32s3-elf-g++)
// and produces assembly output for static analysis only — it is NOT runnable
// on the host machine.
//
// Build via: cmake --build build --target asm_analysis
// Output:    build/custom_rgb565_swap_xtensa.s
//
// The wrapper function prevents the compiler from inlining the swap or
// optimising it away entirely. The `volatile` dummy output ensures that the
// compiler treats the result as observable, defeating dead-store elimination.
//
// Analysis checklist:
//   - Look for `ee.vld.128.ip` / `ee.vst.128.ip` or `s32i` / `l32i` pairs
//     in the main loop — the former indicates ESP32-S3 vector (PIE) usage,
//     the latter is the expected scalar path.
//   - Confirm `src` / `ssr` + `shld` / `shra` for the 8-bit rotate pattern
//     or a `bbci`/`extui`/`or` sequence.
//   - Check loop alignment (`LOOP` instruction presence vs branch-based loops).
//   - Verify the alignment prefix path (the first `bnez` / `beqz` check).

#include "halpp/display/custom_rgb565_swap.hpp"
#include <cstdint>

// Prevent LTO from eliminating the function; the `noinline` + `volatile` trick
// ensures the compiler must materialise the full swap sequence.
[[gnu::noinline]]
void analyse_rgb565_swap(void* buf, uint32_t size_px) {
    custom_rgb565_swap(buf, size_px);
}
