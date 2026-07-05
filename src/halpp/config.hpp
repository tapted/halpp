/**
 * @file config.hpp
 * @brief Zero-overhead, sparsely-overridable configuration for halpp.
 */

#pragma once

#include <cstdint>
#include <soc/gpio_num.h>

namespace HAL::detail {
struct Defaults {
  struct I2CConfig {
    static constexpr gpio_num_t PIN_SDA = GPIO_NUM_8;
    static constexpr gpio_num_t PIN_SCL = GPIO_NUM_9;

    static constexpr uint8_t BUS_NUM = 0;          // I2C_NUM_0
    static constexpr uint32_t CLK_SPEED = 400000;  // 400kHz standard
    static constexpr uint32_t TIMEOUT_MS = 1000;   // Transaction timeout
    static constexpr bool ENABLE_PULLUP = true;
    static constexpr uint32_t SCL_WAIT_US = 0;  // 0 = use default
  };
  struct Display {
    static constexpr bool INVERT_COLORS = true;   // Invert colors (e.g. for OLEDs - white on black)
    static constexpr bool SWAP_XY = false;        // Swap X/Y for portrait vs landscape
    static constexpr bool TRANSPOSE_1BIT = true;  // Transpose displays for LVGL
  };
  struct Display7Seg {
    static constexpr uint8_t I2C_ADDRESS = 0x70;  // Default I2C address for HT16K33
  };
  struct lvgl {
    static constexpr bool DOUBLE_BUFFERED = true;   // Use two buffers for LVGL rendering
    static constexpr uint32_t BUFFER_FRACTION = 1;  // Buffer size = screen_pixels / buffer_fraction
    static constexpr uint32_t TASK_STACK_SIZE = 8192;  // LVGL task stack size
    static constexpr uint32_t TASK_PRIORITY = 5;       // LVGL task priority
    static constexpr uint8_t TASK_CORE_ID = 1;         // LVGL task core affinity
  };
};  // Defaults
}  // namespace HAL::detail

#if __has_include("hal/board.hpp")
#include "hal/board.hpp"
namespace HAL {
// Alias to the user's custom struct
using config = HAL::board::config;
}  // namespace HAL
#else
namespace HAL {
// No board file found? Alias directly to the framework defaults.
using config = HAL::detail::Defaults;
}  // namespace HAL
#endif