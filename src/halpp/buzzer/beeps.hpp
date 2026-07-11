/**
 * @file beeps.hpp
 * @brief Standardized UI interaction sounds for the Passive Buzzer
 */

#pragma once

#include <array>

#include "halpp/buzzer/passive.hpp"

namespace HAL::beeps {

// Helper macro/constexpr for consistent UI volume
constexpr uint8_t kUiVolume = 38;  // 0.15 * 255 ≈ 38

// --- Acknowledge (Short, crisp click/beep for button presses) ---
inline constexpr std::array<Note, 1> acknowledge = {{{2000, 30, kUiVolume}}};

// --- Success Chime (Two rising tones) ---
inline constexpr std::array<Note, 3> success = {{
    {1046, 100, kUiVolume},  // C6
    {0, 50, 0},           // Short pause
    {1568, 150, kUiVolume}   // G6
}};

// --- Error Buzz (Two harsh, low, dissonant tones) ---
inline constexpr std::array<Note, 3> error = {{
    {150, 150, kUiVolume},  // Low buzz
    {0, 50, 0},
    {150, 250, kUiVolume}  // Longer low buzz
}};

// --- Boot/Startup Sequence (Fast rising arpeggio) ---
inline constexpr std::array<Note, 4> startup = {{
    {523, 80, kUiVolume},   // C5
    {659, 80, kUiVolume},   // E5
    {784, 80, kUiVolume},   // G5
    {1046, 150, kUiVolume}  // C6
}};

}  // namespace HAL::beeps