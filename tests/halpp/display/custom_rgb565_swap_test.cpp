// Tests for custom_rgb565_swap — an in-place, byte-swapping function for
// RGB565 pixel buffers.
//
// The function signature is:
//   inline static void custom_rgb565_swap(void* buf, uint32_t buf_size_px)
//
// The implementation has three distinct code paths:
//   1. Alignment prefix  — handles the leading unaligned pixel when the buffer
//                          is 16-bit aligned but NOT 32-bit aligned.
//   2. Main 32-bit loop  — processes pairs of pixels at a time via u32 reads.
//   3. Trailing pixel    — handles the last pixel when buf_size_px is odd.
//
// Every test below documents which path(s) it exercises.

#include "halpp/display/custom_rgb565_swap.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <numeric>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Byte-swap a single 16-bit value — the expected result of one swap.
static uint16_t swap16(uint16_t v) {
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}

/// Build a vector of N sequential uint16_t values starting from `start`.
static std::vector<uint16_t> make_seq(uint32_t n, uint16_t start = 0x0001) {
    std::vector<uint16_t> buf(n);
    std::iota(buf.begin(), buf.end(), start);
    return buf;
}

// ---------------------------------------------------------------------------
// 1. Single pixel  (buf_size_px == 1)
//    Path: trailing-pixel branch only (u32_cnt == 0).
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, SinglePixel) {
    uint16_t pixel = 0xABCD;
    custom_rgb565_swap(&pixel, 1);
    EXPECT_EQ(pixel, 0xCDAB);
}

// ---------------------------------------------------------------------------
// 2. Two pixels — even count, 32-bit aligned
//    Path: main 32-bit loop (u32_cnt == 1, no trailing pixel).
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, TwoPixelsEvenAligned) {
    // alignas(4) guarantees 32-bit alignment.
    alignas(4) uint16_t buf[2] = {0x1234, 0x5678};
    custom_rgb565_swap(buf, 2);
    EXPECT_EQ(buf[0], 0x3412);
    EXPECT_EQ(buf[1], 0x7856);
}

// ---------------------------------------------------------------------------
// 3. Three pixels — odd count, 32-bit aligned
//    Path: main loop (u32_cnt == 1) + trailing-pixel branch.
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, ThreePixelsOddAligned) {
    alignas(4) uint16_t buf[3] = {0xAABB, 0xCCDD, 0xEEFF};
    custom_rgb565_swap(buf, 3);
    EXPECT_EQ(buf[0], 0xBBAA);
    EXPECT_EQ(buf[1], 0xDDCC);
    EXPECT_EQ(buf[2], 0xFFEE);
}

// ---------------------------------------------------------------------------
// 4. Large even buffer (100 pixels), 32-bit aligned
//    Path: main loop only (u32_cnt == 50, no trailing pixel).
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, LargeEvenBufferAligned) {
    constexpr uint32_t N = 100;
    alignas(4) uint16_t buf[N];

    // Fill with sequential values so every pixel is distinct and testable.
    for (uint32_t i = 0; i < N; ++i) buf[i] = static_cast<uint16_t>(0x0100 + i);

    std::vector<uint16_t> expected(N);
    for (uint32_t i = 0; i < N; ++i) expected[i] = swap16(buf[i]);

    custom_rgb565_swap(buf, N);

    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_EQ(buf[i], expected[i]) << "Mismatch at pixel " << i;
    }
}

// ---------------------------------------------------------------------------
// 5. Large odd buffer (101 pixels), 32-bit aligned
//    Path: main loop (u32_cnt == 50) + trailing-pixel branch.
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, LargeOddBufferAligned) {
    constexpr uint32_t N = 101;
    alignas(4) uint16_t buf[N];

    for (uint32_t i = 0; i < N; ++i) buf[i] = static_cast<uint16_t>(0x0200 + i);

    std::vector<uint16_t> expected(N);
    for (uint32_t i = 0; i < N; ++i) expected[i] = swap16(buf[i]);

    custom_rgb565_swap(buf, N);

    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_EQ(buf[i], expected[i]) << "Mismatch at pixel " << i;
    }
}

// ---------------------------------------------------------------------------
// 6. Empty buffer (buf_size_px == 0)
//    Path: none — function should return immediately without touching memory.
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, EmptyBuffer) {
    // Use a sentinel-guarded buffer to catch any out-of-bounds writes.
    alignas(4) uint16_t buf[4] = {0xDEAD, 0xBEEF, 0xCAFE, 0xBABE};
    custom_rgb565_swap(buf, 0);

    // Buffer must be completely unchanged.
    EXPECT_EQ(buf[0], 0xDEAD);
    EXPECT_EQ(buf[1], 0xBEEF);
    EXPECT_EQ(buf[2], 0xCAFE);
    EXPECT_EQ(buf[3], 0xBABE);
}

// ---------------------------------------------------------------------------
// 7. Idempotency — double swap returns original data
//    Applies to all paths; tested here with a medium mixed-parity buffer.
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, DoubleSwapIsIdentity) {
    constexpr uint32_t N = 7;  // odd, so both loop and trailing path execute
    alignas(4) uint16_t buf[N];
    for (uint32_t i = 0; i < N; ++i) buf[i] = static_cast<uint16_t>(0x1357 + i * 0x0111);

    std::vector<uint16_t> original(buf, buf + N);

    custom_rgb565_swap(buf, N);  // first pass
    custom_rgb565_swap(buf, N);  // second pass must undo the first

    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_EQ(buf[i], original[i]) << "Double-swap mismatch at pixel " << i;
    }
}

// ---------------------------------------------------------------------------
// 8. Misaligned buffer — exercises the alignment prefix path
//
//    Strategy: allocate a byte array of size (sizeof(uint16_t)*N + 2).
//    Offset the working pointer by 2 bytes, which yields a pointer that is
//    16-bit aligned (address % 2 == 0) but NOT 32-bit aligned (address % 4 == 2).
//    The function's first branch detects this and byte-swaps the leading pixel
//    before advancing to the 32-bit loop.
//
//    N == 4 gives: 1 prefix pixel + 1 u32 iteration (2 pixels) + 1 trailing pixel.
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, MisalignedBuffer) {
    constexpr uint32_t N = 4;

    // Allocate enough bytes to hold N pixels starting at a +2-byte offset.
    // alignas(4) on the outer array makes buf_storage[0] 32-bit aligned, so
    // buf_storage[2] will be 16-bit aligned but NOT 32-bit aligned.
    alignas(4) uint8_t storage[sizeof(uint16_t) * N + 2] = {};

    // Pointer to our pixel data at offset +2.
    uint8_t* raw = storage + 2;

    // Verify the misalignment assumption holds on this platform.
    ASSERT_EQ(reinterpret_cast<uintptr_t>(raw) % 2, 0u) << "Must be 16-bit aligned";
    ASSERT_EQ(reinterpret_cast<uintptr_t>(raw) % 4, 2u) << "Must NOT be 32-bit aligned";

    // Write known pixel values into the misaligned region.
    const uint16_t pixels[N] = {0xAABB, 0xCCDD, 0x1122, 0x3344};
    std::memcpy(raw, pixels, sizeof(pixels));

    custom_rgb565_swap(raw, N);

    // Read results back.
    uint16_t result[N];
    std::memcpy(result, raw, sizeof(result));

    EXPECT_EQ(result[0], swap16(pixels[0]));
    EXPECT_EQ(result[1], swap16(pixels[1]));
    EXPECT_EQ(result[2], swap16(pixels[2]));
    EXPECT_EQ(result[3], swap16(pixels[3]));
}

// ---------------------------------------------------------------------------
// 9. Known values — explicit pixel-level correctness checks
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, KnownValues) {
    struct TestCase {
        uint16_t input;
        uint16_t expected;
        const char* label;
    };

    // clang-format off
    const TestCase cases[] = {
        {0xABCD, 0xCDAB, "0xABCD"},
        {0x1234, 0x3412, "0x1234"},
        {0xFF00, 0x00FF, "0xFF00"},
        {0x00FF, 0xFF00, "0x00FF"},
        {0x0001, 0x0100, "0x0001"},
        {0x8000, 0x0080, "0x8000"},
        {0x0080, 0x8000, "0x0080"},
        {0xF87E, 0x7EF8, "0xF87E — red in RGB565"},
        {0x07E0, 0xE007, "0x07E0 — green in RGB565"},
        {0x001F, 0x1F00, "0x001F — blue in RGB565"},
    };
    // clang-format on

    for (const auto& tc : cases) {
        alignas(4) uint16_t buf = tc.input;
        custom_rgb565_swap(&buf, 1);
        EXPECT_EQ(buf, tc.expected) << "Failed for " << tc.label;
    }
}

// ---------------------------------------------------------------------------
// 10. All-zeros buffer — swapping zero pixels must remain zero
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, AllZeros) {
    constexpr uint32_t N = 8;
    alignas(4) uint16_t buf[N] = {};  // zero-initialised

    custom_rgb565_swap(buf, N);

    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_EQ(buf[i], 0x0000u) << "Non-zero result at pixel " << i;
    }
}

// ---------------------------------------------------------------------------
// 11. All-0xFFFF buffer — swapping 0xFFFF must remain 0xFFFF (symmetric value)
// ---------------------------------------------------------------------------
TEST(CustomRgb565Swap, AllOnes) {
    constexpr uint32_t N = 8;
    alignas(4) uint16_t buf[N];
    std::fill(buf, buf + N, uint16_t{0xFFFF});

    custom_rgb565_swap(buf, N);

    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_EQ(buf[i], 0xFFFFu) << "Changed value at pixel " << i;
    }
}
