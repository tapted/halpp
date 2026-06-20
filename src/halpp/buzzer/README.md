# Buzzer HAL (`halpp/buzzer`)

This module provides a modern, RAII-based abstraction for passive buzzers using the ESP32 LEDC PWM peripheral. It is designed to be highly efficient, thread-safe, and zero-allocation.

## Overview

The buzzer HAL abstracts the complexity of FreeRTOS task management and LEDC register configuration. It uses a **Tickless State Machine** pattern to play melodies, ensuring that your CPU stays asleep while the music plays, and provides a safe API to interrupt or update playback at any time.

* **RAII Architecture:** Hardware resources (LEDC Timers/Channels) are automatically released when the object is destroyed.
* **Zero-Allocation:** No heap usage (`malloc`/`new`). All melodies are stored in flash as `std::span`.
* **Thread Safety:** Playback is handled by a dedicated background RTOS task, allowing your main application logic to remain non-blocking.

---

## Code Examples

### 1. Simple Initialization

Initialize the buzzer using the default singleton instance.

```cpp
#include "halpp/buzzer/passive.hpp"

// Initialize with a default configuration
HAL::Passive::init_default({
    .gpio_num = GPIO_NUM_13,
    .timer_num = LEDC_TIMER_0,
    .channel = LEDC_CHANNEL_0
});
```

### 2. Playing UI Beeps

For simple system interactions, use the built-in scratchpad or static beep library.

```cpp
#include "halpp/buzzer/beeps.hpp"

// Play a standard success chime
HAL::Passive::default_instance().play(HAL::beeps::success);

// Or generate a dynamic beep on the fly (non-blocking)
HAL::Passive::default_instance().beep(440, 200, 0.5f);
```

### 3. Playing Custom Melodies

Melodies are defined as static arrays of `Note` structs, keeping them in read-only memory.

```cpp
#include "halpp/buzzer/passive.hpp"

// Define a melody in flash (zero RAM footprint)
inline constexpr std::array<HAL::Note, 2> my_melody = {{
    {440, 500, 0.1f}, // A4 for 500ms
    {880, 500, 0.1f}  // A5 for 500ms
}};

HAL::Passive::default_instance().play(my_melody);
```

---

## Architecture & Implementation Notes

### The Lifetime Relationship

The `Passive` buzzer utilizes **Composition** to manage hardware dependencies. The order of declaration in the class is critical:

1. **RTOS Task (`hal::rtos::YieldingTask`):** Declared last. Its destructor fires **first**, ensuring the background thread is terminated before hardware is touched.
2. **Channel (`hal::ledc::Channel`):** Stops the physical PWM signal on the GPIO pin.
3. **Timer (`hal::ledc::Timer`):** Deconfigures the LEDC hardware clock generator.

### Execution Pattern: The Tickless State Machine

Instead of a standard `while(1)` block, the buzzer uses a `YieldingTask`.

* **`playback_step`**: Evaluates the current note, updates hardware, and returns the duration to yield.
* **`playback_stop`**: A universal cleanup handler invoked automatically by the framework when a melody finishes or the user calls `stop()`.

This ensures that the hardware never stays in an "on" state if the software stops, and the CPU is never spinning in a loop while waiting for a note duration to elapse.

### Safety Guarantees

* **Double-Deletion Prevention:** The `YieldingTask` trampoline clears the task handle before exiting, ensuring the RAII destructor doesn't attempt to delete a thread that is already dead.
* **Idempotent Teardown:** Both natural melody completion and explicit `request_stop()` triggers fire the same cleanup logic, ensuring the buzzer can never be left in a buzzing state.