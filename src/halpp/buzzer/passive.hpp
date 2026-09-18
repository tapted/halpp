/**
 * @file passive.hpp
 * @brief Passive Buzzer (PWM/LEDC) HAL wrapper
 * @details Based on esp_idf_buzzer by chendi (cdsama) under MIT License.
 * Refactored for halpp zero-allocation architecture.
 */

#pragma once

#include <cstdint>
#include <hal/ledc_types.h>
#include <optional>
#include <soc/gpio_num.h>
#include <span>

#include "espbase/esp_result.hpp"
#include "espbase/yielding_task.hpp"
#include "halpp/config.hpp"
#include "halpp/core/default_instance.hpp"
#include "halpp/ledc/channel.hpp"
#include "halpp/ledc/timer.hpp"

namespace halpp {

struct Note {
  uint16_t frequency_hz = 4000;
  uint16_t duration_ms = 500;
  uint8_t volume_255 = 25;  // Range: 0 ~ 255
};

// A non-owning view of a sequence of notes.
// WARNING: The underlying memory backing this span must outlive the playback duration!
using Melody = std::span<const Note>;

class Passive : public DefaultInstance<Passive> {
 public:
  struct Config {
    gpio_num_t gpio_num = config::Buzzer::PIN_PWM;
    ledc_timer_t timer_num = config::Buzzer::LEDC_TIMER;
    ledc_channel_t channel = config::Buzzer::LEDC_CHANNEL;
#ifdef HALPP_USE_XTAL_FOR_BUZZER
    ledc_clk_cfg_t clk_cfg = LEDC_USE_XTAL_CLK;
    ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT;
#else
    // Default to RC_FAST. The APB clock is affected by light sleep and frequency scaling. XTAL is
    // fixed at 40MHz which gives a max tone of 4,882Hz with 13-bit resolution. The APB clock would
    // double that, but it would be affected by power management. But XTAL wants to power down
    // during light sleep. RC_FAST is 17.5MHz (approximately - it drifts), so not great for music
    // but OK for buzzers.
    ledc_clk_cfg_t clk_cfg = LEDC_USE_RC_FAST_CLK;
    ledc_timer_bit_t timer_bit = LEDC_TIMER_10_BIT;
#endif
    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;

    uint32_t idle_level = 0;
  };

  explicit Passive(Config config) : config_(config) {}
  ~Passive();

  // Executes hardware initialization and spawns background FreeRTOS task
  EspResult<> init();

  // --- Playback Operations ---
  bool is_initialized() const { return !!pwm_timer_; }

  // Plays a melody asynchronously. The backing array must remain valid in memory.
  void play(Melody melody);
  // Plays a single, dynamically defined note.
  void beep(uint16_t frequency_hz, uint16_t duration_ms, float volume = 0.1f);
  void stop();

 private:
  // The isolated payload struct for the FreeRTOS task
  struct PlaybackState {
    const Note* melody = nullptr;
    size_t size = 0;
    size_t current_index = 0;   // State Machine offset tracking
    Passive* buzzer = nullptr;  // Allows the static loop to access hardware methods
  };

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

}  // namespace halpp