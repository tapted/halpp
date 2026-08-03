#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <soc/gpio_num.h>

#include "espbase/esp_result.hpp"

#include <hal/gpio_types.h>

namespace halpp::gpio {

class DebouncedInput {
 public:
  struct Config {
    uint32_t debounce_ms = 100;
    gpio_int_type_t intr_type = GPIO_INTR_ANYEDGE;

    // Useful for buttons or sensors without hardware pullups
    gpio_pull_mode_t pull_mode = GPIO_FLOATING;

    // Invoked on the main task stack when the pin settles
    void (*on_changed)(bool state, void* ctx) = nullptr;
    void* ctx = nullptr;
  };

  explicit constexpr DebouncedInput(gpio_num_t pin) : pin_(pin) {}
  ~DebouncedInput();

  EspResult<> begin(Config config);

  // Manual read capability
  bool get_level() const;

 private:
  gpio_num_t pin_;
  void (*on_changed_)(bool state, void* ctx) = nullptr;
  void* ctx_ = nullptr;
  TimerHandle_t debounce_timer_ = nullptr;
  bool isr_installed_ = false;

  // Track the last known stable state to prevent phantom callbacks
  bool last_stable_state_ = false;

  DebouncedInput(const DebouncedInput&) = delete;
  DebouncedInput& operator=(const DebouncedInput&) = delete;
  DebouncedInput(DebouncedInput&&) = delete;
  DebouncedInput& operator=(DebouncedInput&&) = delete;

  // The hardware-context ISR (Must be static, must be in IRAM)
  static void IRAM_ATTR isr_handler(void* arg);

  // The software-context timer callback
  static void timer_callback(TimerHandle_t xTimer);
};

}  // namespace halpp::gpio