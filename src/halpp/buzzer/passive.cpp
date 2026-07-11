/**
 * @file passive.cpp
 * @brief Passive Buzzer (PWM/LEDC) HAL wrapper
 * @details Ported from esp_idf_buzzer by chendi (MIT License)
 * Original Source: https://github.com/cdsama/esp_idf_buzzer
 */

#include "halpp/buzzer/passive.hpp"

#include <algorithm>
#include <driver/ledc.h>

#include <hal/ledc_ll.h>

namespace HAL {

static const char* TAG = "PassiveBuzzer";

Passive::~Passive() {
}

EspResult<void> Passive::init_default(Config config) {
  std::optional<Passive>& opt = default_optional();
  if (!opt) {
    opt.emplace(config);
  }
  return opt->begin();
}

EspResult<void> Passive::begin() {
  if (is_initialized()) return ESP_OK;

  // 1. Configure the hardware timer and take ownership
  if (EspError err = EspError::check(Timer::configure(config_.timer_num, config_.clk_cfg,
                                                      config_.timer_bit, 4000, config_.speed_mode),
                                     &pwm_timer_)) {
    return err.log(TAG, "Failed to configure LEDC timer");
  }

  // 2. Attach a GPIO channel to the timer and take ownership
  if (EspError err = EspError::check(
          pwm_timer_.add_channel(config_.channel, config_.gpio_num, config_.idle_level),
          &pwm_channel_)) {
    return err.log(TAG, "Failed to configure LEDC channel");
  }

  pwm_channel_.stop();  // Halt any immediate hardware jitter

  // 3. Prepare Payload and Spawn Task
  playback_state_.buzzer = this;

  if (EspError err = task_.start(
          {
              .name = "buzzer",
              .priority = uxTaskPriorityGet(nullptr),
              .prevent_light_sleep = true,
          },
          &playback_state_, playback_step, playback_stop)) {
    return err.log(TAG, "Failed to spawn buzzer FreeRTOS task");
  }
  return ESP_OK;
}

void Passive::play(Melody melody) {
  if (!is_initialized()) return;

  playback_state_.melody = melody.data();
  playback_state_.size = melody.size();
  playback_state_.current_index = 0;  // Reset state machine for new playback

  task_.notify(true);  // Wake the task up and clear any existing stop requests
}

void Passive::beep(uint16_t frequency_hz, uint16_t duration_ms, float volume) {
  if (!is_initialized()) return;

  uint8_t volume_255 = static_cast<uint8_t>(std::clamp(volume, 0.0f, 1.0f) * 255);
  // 1. Write the dynamic values into our stable, instance-owned memory
  beep_scratchpad_[0] = Note{frequency_hz, duration_ms, volume_255};

  // 2. Play it. The span safely points to the scratchpad.
  play(beep_scratchpad_);
}

void Passive::stop() {
  if (!is_initialized()) return;
  task_.request_stop();
}

std::optional<uint32_t> Passive::playback_step(YieldingTask<PlaybackState>& task) {
  PlaybackState* state = task.data();
  if (state->current_index >= state->size || state->melody == nullptr) {
    return std::nullopt;  // Executes playback_stop().
  }

  const Note& note = state->melody[state->current_index];
  state->buzzer->set_hardware_note(note.frequency_hz, 1.0f * note.volume_255 / 255);
  state->current_index++;
  return note.duration_ms;
}

void Passive::playback_stop(YieldingTask<PlaybackState>& task) {
  task.data()->buzzer->pwm_channel_.stop();
}

void Passive::set_hardware_note(uint32_t frequency, float volume) {
  if (frequency == 0 || volume <= 0.0f) {
    pwm_channel_.stop();
    return;
  }
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
  constexpr uint32_t rc_fast_clock_hz = 17500000;  // ~17.5 MHz
#else
  constexpr uint32_t rc_fast_clock_hz = 8500000;
#endif
  uint32_t source_clock_hz = 0;
  if (config_.clk_cfg == LEDC_USE_RC_FAST_CLK) {
    source_clock_hz = rc_fast_clock_hz;
  } else if (config_.clk_cfg == LEDC_USE_APB_CLK) {
    source_clock_hz = 80000000;  // 80 MHz
  } else if (config_.clk_cfg == LEDC_USE_XTAL_CLK) {
    source_clock_hz = 40000000;  // XTAL is 40 MHz
  } else {
    source_clock_hz = rc_fast_clock_hz;  // possibly LEDC_AUTO_CLK - be conservative
  }

  // Dynamically calculate the absolute hardware ceiling
  uint32_t max_hw_freq = source_clock_hz / (1 << config_.timer_bit);

  // Clamp safely between acoustic floor and hardware ceiling
  frequency = std::clamp<uint32_t>(frequency, 20, max_hw_freq);

  // Acoustic Duty Cycle Math (50% maximum = peak volume)
  volume = std::clamp(volume, 0.0f, 1.0f);
  uint32_t max_acoustic_duty = (1 << config_.timer_bit) / 2;
  uint32_t duty = max_acoustic_duty * volume;

  pwm_channel_.set_duty(duty);
  pwm_channel_.update_duty();
  pwm_timer_.set_freq(frequency);
}

}  // namespace HAL