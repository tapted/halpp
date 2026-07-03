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

// Acoustic Frequencies (Equal Temperament)
constexpr uint32_t REST = 0;
constexpr uint32_t N_FS4 = 370; // F#4
constexpr uint32_t N_A4  = 440; // A4
constexpr uint32_t N_B4  = 494; // B4
constexpr uint32_t N_CS5 = 554; // C#5
constexpr uint32_t N_D5  = 587; // D5
constexpr uint32_t N_E5  = 659; // E5

// Imagine Dragons - Radioactive (Chorus)
// Tempo: ~136 BPM. 
// Note: We use 180ms play + 40ms rest for 8th notes to force the piezo to 
// physically stop vibrating, creating the "staccato" vocal articulation.
constexpr std::array<Note, 52> radioactive_riff = {{
    // "I'm wak-ing up..."
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    
    // "to ash and dust"
    { N_B4,  180 }, { REST, 40 },
    { N_A4,  180 }, { REST, 40 },
    { N_FS4, 180 }, { REST, 40 },
    { N_A4,  400 }, { REST, 40 },
    { REST,  200 }, // Breath pause
    
    // "I wipe my brow..."
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    
    // "and I sweat my rust"
    { N_B4,  180 }, { REST, 40 },
    { N_A4,  180 }, { REST, 40 },
    { N_FS4, 180 }, { REST, 40 },
    { N_A4,  180 }, { REST, 40 },
    { N_B4,  400 }, { REST, 40 },
    { REST,  200 }, // Breath pause

    // "I'm breath-ing in..."
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },
    { N_B4, 180 }, { REST, 40 },

    // "the chem-i-cals..."
    { N_D5,  200 }, { REST, 20 },
    { N_D5,  200 }, { REST, 20 },
    { N_CS5, 200 }, { REST, 20 },
    { N_B4,  600 }, { REST, 100 }
}};

}  // namespace HAL::melodies