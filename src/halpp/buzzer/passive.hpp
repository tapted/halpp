/**
 * @file passive.hpp
 * @brief Passive Buzzer (PWM/LEDC) HAL wrapper
 * @details Based on esp_idf_buzzer by chendi (cdsama) under MIT License.
 * Refactored for halpp zero-allocation architecture.
 */

#pragma once

#include <cstdint>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <optional>
#include <span>

#include "espbase/esp_result.hpp"
#include "espbase/yielding_task.hpp"
#include "halpp/ledc/channel.hpp"
#include "halpp/ledc/timer.hpp"

namespace HAL {

struct Note {
  uint32_t frequency_hz = 4000;
  uint32_t duration_ms = 500;
  float volume = 0.1f;  // Range: 0.0f ~ 1.0f
};

// A non-owning view of a sequence of notes.
// WARNING: The underlying memory backing this span must outlive the playback duration!
using Melody = std::span<const Note>;

class Passive {
 public:
  struct Config {
    gpio_num_t gpio_num;
    ledc_timer_t timer_num = LEDC_TIMER_0;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT;
    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;
    uint32_t idle_level = 0;
  };

  // --- Pattern 1: Multi-Device (Explicit Ownership) ---
  explicit Passive(Config config) : config_(config) {}
  ~Passive();

  // --- Pattern 2: Single-Device Default (init_default must be called first) ---
  static Passive& default_instance() { return *default_optional(); }
  static EspResult<void> init_default(Config config);
  static EspResult<void> deinit_default();

  // Executes hardware initialization and spawns background FreeRTOS task
  EspResult<void> begin();

  // --- Playback Operations ---
  bool is_initialized() const { return !!pwm_timer_; }

  // Plays a melody asynchronously. The backing array must remain valid in memory.
  void play(Melody melody);
  // Plays a single, dynamically defined note.
  void beep(uint32_t frequency_hz, uint32_t duration_ms, float volume = 0.1f);
  void stop();

 private:
  // The isolated payload struct for the FreeRTOS task
  struct PlaybackState {
    const Note* melody = nullptr;
    size_t size = 0;
    size_t current_index = 0;   // State Machine offset tracking
    Passive* buzzer = nullptr;  // Allows the static loop to access hardware methods
  };

  static std::optional<Passive>& default_optional() {
    static std::optional<Passive> inst;
    return inst;
  }

  Passive(const Passive&) = delete;
  Passive& operator=(const Passive&) = delete;

  Config config_;
  PlaybackState playback_state_{};

  Timer pwm_timer_;
  Channel pwm_channel_;
  YieldingTask<PlaybackState> task_;

  // A 12-byte permanent memory location to safely back the std::span when playing a dynamically
  // generated runtime beep.
  std::array<Note, 1> beep_scratchpad_{};

  static std::optional<uint32_t> playback_step(YieldingTask<PlaybackState>& task);
  static void playback_stop(YieldingTask<PlaybackState>& task);
  void set_hardware_note(uint32_t frequency_hz, float volume);
};

}  // namespace HAL