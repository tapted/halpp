#include "debounced_input.hpp"

#include <esp_log.h>

namespace halpp::gpio {

EspResult<> DebouncedInput::begin(Config config) {
  pin_ = config.pin;
  on_changed_ = config.on_changed;
  ctx_ = config.ctx;

  // 1. Configure the GPIO hardware
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << config.pin);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en =
      (config.pull_mode == GPIO_PULLUP_ONLY || config.pull_mode == GPIO_PULLUP_PULLDOWN)
          ? GPIO_PULLUP_ENABLE
          : GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en =
      (config.pull_mode == GPIO_PULLDOWN_ONLY || config.pull_mode == GPIO_PULLUP_PULLDOWN)
          ? GPIO_PULLDOWN_ENABLE
          : GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = config.intr_type;
  gpio_config(&io_conf);

  // 2. Create the FreeRTOS Timer
  // We pass 'this' into the timer ID field so the static callback can resolve the instance
  debounce_timer_ = xTimerCreate("debounce_tmr", pdMS_TO_TICKS(config.debounce_ms),
                                 pdFALSE,  // One-shot mode
                                 this,     // Bind instance to Timer ID
                                 timer_callback);

  // 3. Install the ISR Service
  // We ignore ESP_ERR_INVALID_STATE, which just means another driver already installed the global
  // service
  if (EspError err = gpio_install_isr_service(0)) {
    if (err != ESP_ERR_INVALID_STATE) {
      return err.log("DebouncedInput", "Failed to install ISR service");
    }
  }

  // 4. Attach the pin to our static handler, passing 'this' as the argument
  if (EspError err = gpio_isr_handler_add(config.pin, isr_handler, this)) {
    return err.log("DebouncedInput", "Failed to add ISR handler for GPIO");
  }
  isr_installed_ = true;
  last_stable_state_ = get_level();  // Seed the initial baseline state
  return ESP_OK;
}

DebouncedInput::~DebouncedInput() {
  // Graceful teardown in reverse order of construction
  if (isr_installed_) {
    gpio_isr_handler_remove(pin_);
  }

  if (debounce_timer_) {
    // Block up to portMAX_DELAY to ensure the timer is fully deleted from FreeRTOS memory
    xTimerDelete(debounce_timer_, portMAX_DELAY);
  }

  gpio_reset_pin(pin_);
}

void IRAM_ATTR DebouncedInput::isr_handler(void* arg) {
  // Hardware Context: Cast the arg back to our object instance
  DebouncedInput* self = static_cast<DebouncedInput*>(arg);

  if (!self->debounce_timer_) return;

  BaseType_t higher_priority_task_woken = pdFALSE;
  xTimerResetFromISR(self->debounce_timer_, &higher_priority_task_woken);

  if (higher_priority_task_woken) {
    portYIELD_FROM_ISR();
  }
}

void DebouncedInput::timer_callback(TimerHandle_t xTimer) {
  // Software Context: Extract our object instance from the Timer ID
  DebouncedInput* self = static_cast<DebouncedInput*>(pvTimerGetTimerID(xTimer));
  if (!self) return;

  bool current_state = self->get_level();
  if (current_state != self->last_stable_state_) {
    self->last_stable_state_ = current_state;

    if (self->on_changed_) {
      self->on_changed_(current_state, self->ctx_);
    }
  }
}

}  // namespace halpp::gpio