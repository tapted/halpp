/**
 * @file passive.cpp
 * @brief Passive Buzzer (PWM/LEDC) HAL wrapper
 * @details Ported from esp_idf_buzzer by chendi (MIT License)
 * Original Source: https://github.com/cdsama/esp_idf_buzzer
 */

#include "passive.hpp"

#include <algorithm>
#include <optional>

#include "esp_log.h"

#include "hal/ledc_ll.h"

namespace HAL {

static const char* TAG = "PassiveBuzzer";
static const uint32_t STACK_SIZE = 2048;

Passive::~Passive() {
  if (task_handle_ != nullptr) {
    stop_requested_ = true;
    xSemaphoreGive(sync_sem_);
    vTaskDelete(task_handle_);
  }
  if (sync_sem_ != nullptr) {
    vSemaphoreDelete(sync_sem_);
  }
}

EspResult<void> Passive::init_default(Config config) {
  std::optional<Passive>& opt = default_optional();
  if (!opt) {
    opt.emplace(config);
  }
  return opt->begin();
}

EspResult<void> Passive::deinit_default() {
  default_optional().reset();
  return ESP_OK;
}

EspResult<void> Passive::begin() {
  if (is_initialized()) return ESP_OK;

  // 1. Configure the hardware timer and take ownership
  if (EspError err = EspError::check(Timer::configure(config_.speed_mode, config_.timer_num,
                                                      config_.timer_bit, 4000, config_.clk_cfg),
                                     &pwm_timer_)) {
    return err.log(TAG, "Failed to configure LEDC timer");
  }

  // 2. Attach a GPIO channel to the timer and take ownership
  if (EspError err = EspError::check(
          pwm_timer_.add_channel(config_.channel, config_.gpio_num, config_.idle_level),
          &pwm_channel_)) {
    return err.log(TAG, "Failed to configure LEDC channel");
  }

  // Halt any immediate hardware jitter
  pwm_channel_.stop();

  // 3. Create Sync Sem and FreeRTOS Task
  sync_sem_ = xSemaphoreCreateBinary();
  if (!sync_sem_) return ESP_ERR_NO_MEM;

  BaseType_t res = xTaskCreate(task_trampoline, "buzzer_task", STACK_SIZE, this,
                               uxTaskPriorityGet(nullptr), &task_handle_);
  if (res != pdPASS) return ESP_ERR_NO_MEM;

  return ESP_OK;
}

void Passive::play(Melody melody) {
  if (!is_initialized()) return;

  current_melody_ = melody.data();
  melody_size_ = melody.size();
  stop_requested_ = false;
  xSemaphoreGive(sync_sem_);
}

void Passive::stop() {
  if (!is_initialized()) return;

  stop_requested_ = true;
  xSemaphoreGive(sync_sem_);
}

void Passive::task_trampoline(void* arg) {
  static_cast<Passive*>(arg)->task_loop();
}

void Passive::task_loop() {
  while (true) {
    if (xSemaphoreTake(sync_sem_, portMAX_DELAY) == pdTRUE) {
      bool restart = true;
      while (restart) {
        restart = false;
        const Note* playing = current_melody_;
        size_t size = melody_size_;

        if (playing != nullptr && !stop_requested_) {
          for (size_t i = 0; i < size; ++i) {
            if (stop_requested_) break;

            set_hardware_note(playing[i].frequency_hz, playing[i].volume);

            if (xSemaphoreTake(sync_sem_, pdMS_TO_TICKS(playing[i].duration_ms)) == pdTRUE) {
              if (!stop_requested_) restart = true;
              break;
            }
          }
        }
        // Graceful pause between melodies or when stopped
        pwm_channel_.stop();
      }
    }
  }
}

void Passive::set_hardware_note(uint32_t frequency, float volume) {
  if (frequency == 0 || volume <= 0.0f) {
    pwm_channel_.stop();
    return;
  }

  frequency =
      std::clamp<uint32_t>(frequency, 1ul << LEDC_LL_FRACTIONAL_BITS, 1ul << config_.timer_bit);
  volume = std::clamp(volume, 0.0f, 1.0f);

  uint32_t duty = (1 << config_.timer_bit) * volume;

  pwm_channel_.set_duty(duty);
  pwm_channel_.update_duty();
  pwm_timer_.set_freq(frequency);
}

}  // namespace HAL