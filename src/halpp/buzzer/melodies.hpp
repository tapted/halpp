/**
 * @file melodies.hpp
 * @brief Pre-compiled music arrays for the Passive Buzzer
 */

#pragma once

#include <array>

#include "halpp/buzzer/passive.hpp"

namespace HAL::melodies {

namespace internal {
constexpr uint32_t freq_map[] = {
    0,                                        // 0
    262,  294,  330,  349,  392,  440,  494,  // 1-7 (low)
    523,  587,  659,  698,  784,  880,  988,  // 8-14 (mid)
    1046, 1175, 1318, 1397, 1568, 1760, 1976  // 15-21 (high)
};

struct RawNote {
  uint8_t pitch_idx;
  uint8_t duration_units;
};

// The base note sequence for "Mo Li Hua" (Jasmine Flower)
// Indices: +0 (Low), +7 (Mid), +14 (High)
constexpr RawNote mo_li_hua_raw[] = {
    {3 + 7, 4}, {3 + 7, 2},  {5 + 7, 2},  {6 + 7, 2}, {1 + 14, 2}, {1 + 14, 2}, {6 + 7, 2},
    {5 + 7, 4}, {5 + 7, 2},  {6 + 7, 2},  {5 + 7, 8}, {3 + 7, 4},  {3 + 7, 2},  {5 + 7, 2},
    {6 + 7, 2}, {1 + 14, 2}, {1 + 14, 2}, {6 + 7, 2}, {5 + 7, 4},  {5 + 7, 2},  {6 + 7, 2},
    {5 + 7, 8}, {5 + 7, 4},  {5 + 7, 4},  {5 + 7, 4}, {3 + 7, 2},  {5 + 7, 2},  {6 + 7, 4},
    {6 + 7, 4}, {5 + 7, 8},  {3 + 7, 4},  {2 + 7, 2}, {3 + 7, 2},  {5 + 7, 4},  {3 + 7, 2},
    {2 + 7, 2}, {1 + 7, 4},  {1 + 7, 2},  {2 + 7, 2}, {1 + 7, 8},  {3 + 7, 2},  {2 + 7, 2},
    {1 + 7, 2}, {3 + 7, 2},  {2 + 7, 6},  {3 + 7, 2}, {5 + 7, 4},  {6 + 7, 2},  {1 + 14, 2},
    {5 + 7, 8}, {2 + 7, 4},  {3 + 7, 2},  {5 + 7, 2}, {2 + 7, 2},  {3 + 7, 2},  {1 + 7, 2},
    {6, 2},     {5, 8},      {6, 4},      {1 + 7, 4}, {2 + 7, 6},  {3 + 7, 2},  {1 + 7, 2},
    {2 + 7, 2}, {1 + 7, 2},  {6, 2},      {5, 10}};

constexpr size_t mo_li_hua_len = sizeof(mo_li_hua_raw) / sizeof(RawNote);

// Compile-time builder to interleave notes with a 10ms hardware pause
constexpr auto build_mo_li_hua() {
  std::array<Note, mo_li_hua_len * 2> arr{};
  for (size_t i = 0; i < mo_li_hua_len; ++i) {
    arr[i * 2] = Note{
        freq_map[mo_li_hua_raw[i].pitch_idx],
        static_cast<uint32_t>(150 * mo_li_hua_raw[i].duration_units),
        0.01f,
    };
    // Inter-note pause
    arr[i * 2 + 1] = Note{
        0,
        10,
        0.0f,
    };
  }
  return arr;
}
}  // namespace internal

inline constexpr auto mo_li_hua = internal::build_mo_li_hua();

}  // namespace HAL::melodies