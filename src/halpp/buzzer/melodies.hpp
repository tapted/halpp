/**
 * @file melodies.hpp
 * @brief Pre-compiled music arrays for the Passive Buzzer
 * Defines melodies:
 * HAL::melodies::mo_li_hua
 * HAL::melodies::radioactive_riff
 * HAL::melodies::shanty_riff
 * HAL::melodies::limit_test_chord
 * HAL::melodies::korobeiniki
 * HAL::melodies::korobeiniki_riff
 * HAL::melodies::ambient_sequence
 * HAL::melodies::factory_drone 
 */

#pragma once

#include <array>

#include "halpp/buzzer/passive.hpp"

namespace HAL::melodies {

namespace internal {
constexpr uint16_t freq_map[] = {
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
        static_cast<uint16_t>(150 * mo_li_hua_raw[i].duration_units),
        3,
    };
    // Inter-note pause
    arr[i * 2 + 1] = Note{
        0,
        10,
        0,
    };
  }
  return arr;
}
}  // namespace internal

inline constexpr auto mo_li_hua = internal::build_mo_li_hua();

// Acoustic Frequencies (Equal Temperament)
constexpr uint32_t REST = 0;
constexpr uint32_t N_FS4 = 370;  // F#4
constexpr uint32_t N_CS5 = 554;  // C#5
constexpr uint32_t N_D5 = 587;   // D5
constexpr uint32_t N_E5 = 659;   // E5
constexpr uint32_t N_E4 = 330;   // E4 - Lower harmony anchor.
constexpr uint32_t N_G4 = 392;   // G4 - Passing tone.
constexpr uint32_t N_A4 = 440;   // A4 - The root note of the A Minor scale.
constexpr uint32_t N_B4 = 494;   // B4 - Tension builder.
constexpr uint32_t N_C5 = 523;   // C5 - Peak melody note for the chorus.
constexpr uint32_t N_GS4 = 415;  // G#4 (Leading tone for A Minor).
constexpr uint32_t N_F5 = 698;   // F5

// Imagine Dragons - Radioactive (Chorus)
// Tempo: ~136 BPM.
// Note: We use 180ms play + 40ms rest for 8th notes to force the piezo to
// physically stop vibrating, creating the "staccato" vocal articulation.
constexpr std::array<Note, 52> radioactive_riff = {{
    // "I'm wak-ing up..."
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},

    // "to ash and dust"
    {N_B4, 180},
    {REST, 40},
    {N_A4, 180},
    {REST, 40},
    {N_FS4, 180},
    {REST, 40},
    {N_A4, 400},
    {REST, 40},
    {REST, 200},  // Breath pause

    // "I wipe my brow..."
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},

    // "and I sweat my rust"
    {N_B4, 180},
    {REST, 40},
    {N_A4, 180},
    {REST, 40},
    {N_FS4, 180},
    {REST, 40},
    {N_A4, 180},
    {REST, 40},
    {N_B4, 400},
    {REST, 40},
    {REST, 200},  // Breath pause

    // "I'm breath-ing in..."
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},
    {N_B4, 180},
    {REST, 40},

    // "the chem-i-cals..."
    {N_D5, 200},
    {REST, 20},
    {N_D5, 200},
    {REST, 20},
    {N_CS5, 200},
    {REST, 20},
    {N_B4, 600},
    {REST, 100},
}};

// Sea Shanty (Wellerman Chorus Variant)
// Tempo: ~100 BPM.
// Uses volume dynamics to create heavy downbeat accents.
constexpr uint8_t VOL_NORM = 102;  // 0.4 * 255
constexpr uint8_t VOL_ACC = 255;   // 1.0 * 255

constexpr std::array<Note, 34> shanty_riff = {{
    // "Soon may the We-ller-man come"
    {N_E4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_A4, 250, VOL_ACC},
    {REST, 50, 0},  // Heavy Accent
    {N_A4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_A4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_A4, 250, VOL_ACC},
    {REST, 50, 0},  // Heavy Accent
    {N_B4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_C5, 300, VOL_ACC},
    {REST, 100, 0},  // "-man"
    {N_A4, 250, VOL_NORM},
    {REST, 100, 0},  // "come"

    // "To bring us su-gar and tea and rum"
    {N_E4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_G4, 250, VOL_ACC},
    {REST, 50, 0},  // Heavy Accent
    {N_G4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_G4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_A4, 250, VOL_ACC},
    {REST, 50, 0},  // Heavy Accent
    {N_B4, 150, VOL_NORM},
    {REST, 50, 0},
    {N_C5, 200, VOL_NORM},
    {REST, 50, 0},
    {N_C5, 200, VOL_NORM},
    {REST, 50, 0},
    {N_A4, 500, VOL_ACC},
    {REST, 200, 0}  // "rum"
}};

// Arpeggiated C Major Chord Fade-Out
// Tests rapid frequency switching (30ms) and PWM duty cycle limits.
// C5 (523), E5 (659), G5 (784)
constexpr std::array<Note, 12> limit_test_chord = {{
    {523, 30, 255},
    {659, 30, 255},
    {784, 30, 255},  // Loud
    {523, 30, 153},
    {659, 30, 153},
    {784, 30, 153},  // Medium
    {523, 30, 51},
    {659, 30, 51},
    {784, 30, 51},  // Quiet
    {523, 30, 13},
    {659, 30, 13},
    {784, 100, 13}  // Fade out
}};

// Korobeiniki (Tetris Theme - First 4 Bars)
// Tempo: Fast and rigid.
// Requires crisp hardware pauses to prevent the notes from bleeding together.
constexpr std::array<Note, 40> korobeiniki = {{
    {N_E5, 200, 128}, {REST, 50, 0}, {N_B4, 100, 128}, {REST, 25, 0},
    {N_C5, 100, 128}, {REST, 25, 0}, {N_D5, 200, 128}, {REST, 50, 0},
    {N_C5, 100, 128}, {REST, 25, 0}, {N_B4, 100, 128}, {REST, 25, 0},

    {N_A4, 200, 128}, {REST, 50, 0}, {N_A4, 100, 128}, {REST, 25, 0},
    {N_C5, 100, 128}, {REST, 25, 0}, {N_E5, 200, 128}, {REST, 50, 0},
    {N_D5, 100, 128}, {REST, 25, 0}, {N_C5, 100, 128}, {REST, 25, 0},

    {N_B4, 200, 128}, {REST, 50, 0}, {N_B4, 100, 128}, {REST, 25, 0},
    {N_C5, 100, 128}, {REST, 25, 0}, {N_D5, 200, 128}, {REST, 50, 0},

    {N_E5, 200, 128}, {REST, 50, 0}, {N_C5, 200, 128}, {REST, 50, 0},
    {N_A4, 200, 128}, {REST, 50, 0}, {N_A4, 400, 128}, {REST, 100, 0},
}};

constexpr uint8_t V_K = 100;  // Standard volume

constexpr std::array<Note, 42> korobeiniki_riff = {{
    // Measure 1
    {N_E5, 270, V_K},
    {REST, 30, 0},
    {N_B4, 120, V_K},
    {REST, 30, 0},
    {N_C5, 120, V_K},
    {REST, 30, 0},
    {N_D5, 270, V_K},
    {REST, 30, 0},

    // Measure 2
    {N_C5, 120, V_K},
    {REST, 30, 0},
    {N_B4, 120, V_K},
    {REST, 30, 0},
    {N_A4, 270, V_K},
    {REST, 30, 0},
    {N_A4, 120, V_K},
    {REST, 30, 0},

    // Measure 3
    {N_C5, 120, V_K},
    {REST, 30, 0},
    {N_E5, 270, V_K},
    {REST, 30, 0},
    {N_D5, 120, V_K},
    {REST, 30, 0},
    {N_C5, 120, V_K},
    {REST, 30, 0},

    // Measure 4
    {N_B4, 270, V_K},
    {REST, 30, 0},
    {N_B4, 120, V_K},
    {REST, 30, 0},
    {N_C5, 120, V_K},
    {REST, 30, 0},
    {N_D5, 270, V_K},
    {REST, 30, 0},

    // Measure 5
    {N_E5, 270, V_K},
    {REST, 30, 0},
    {N_C5, 270, V_K},
    {REST, 30, 0},
    {N_A4, 270, V_K},
    {REST, 30, 0},
    {N_A4, 270, V_K},
    {REST, 30, 0},
    {REST, 300, 0},
}};

namespace internal {

constexpr size_t SWELL_STEPS = 40;  // 40 steps up, 40 steps down
constexpr uint32_t SWELL_TICK_MS = 25;
constexpr float MAX_SWELL_VOL = 0.25f;  // Keep it quiet to reduce square wave harshness

// Compile-time builder for a single ambient breathing note
constexpr auto build_swell(uint16_t freq_hz) {
  std::array<Note, SWELL_STEPS * 2> swell{};

  // Fade In
  for (size_t i = 0; i < SWELL_STEPS; ++i) {
    float vol = (static_cast<float>(i + 1) / SWELL_STEPS) * MAX_SWELL_VOL;
    uint8_t volume_255 = static_cast<uint8_t>(vol * 255);
    swell[i] = Note{freq_hz, SWELL_TICK_MS, volume_255};
  }

  // Fade Out
  for (size_t i = 0; i < SWELL_STEPS; ++i) {
    float vol = MAX_SWELL_VOL - ((static_cast<float>(i) / SWELL_STEPS) * MAX_SWELL_VOL);
    uint8_t volume_255 = static_cast<uint8_t>(vol * 255);
    swell[SWELL_STEPS + i] = Note{freq_hz, SWELL_TICK_MS, volume_255};
  }

  return swell;
}

// Stitching multiple swells together into a Lydian progression
constexpr auto build_ambient_sequence() {
  constexpr auto swell1 = build_swell(N_F5);
  constexpr auto swell2 = build_swell(N_E5);
  constexpr auto swell3 = build_swell(N_C5);

  std::array<Note, (SWELL_STEPS * 2) * 3> seq{};

  for (size_t i = 0; i < swell1.size(); ++i) {
    seq[i] = swell1[i];
    seq[swell1.size() + i] = swell2[i];
    seq[(swell1.size() * 2) + i] = swell3[i];
  }
  return seq;
}

}  // namespace internal

inline constexpr auto ambient_sequence = internal::build_ambient_sequence();

// Factory Drone Swell
// Simulates a large industrial machine spinning up, peaking, and spinning down.
// Tests the YieldingTask's ability to step volume cleanly over long durations.
constexpr std::array<Note, 8> factory_drone = {{
    {65, 500, 13},  // Spin up (low duty cycle)
    {65, 500, 50},
    {65, 500, 150},
    {65, 500, 255},  // Peak resonance
    {65, 500, 150},
    {65, 500, 50},  // Spin down
    {65, 500, 13},
    {0, 1500, 0}  // Long silence before the loop repeats
}};

}  // namespace HAL::melodies