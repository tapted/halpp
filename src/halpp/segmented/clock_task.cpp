#include "halpp/segmented/clock_task.hpp"

#include "halpp/segmented/i2c_7seg.hpp"

static std::optional<uint32_t> clock_update_step(YieldingTask<int>&) {
  return HAL::I2C7Seg::default_instance().show_time();
}

void ClockTask::on_time_synced() {
  if (task_.running()) {
    task_.notify();
  } else {
    // Empricially, we need 108 bytes right now. 512 is plenty. Just don't log in the task thread.
    // Any call to logging needs ~1.5kB and will instantly crash the task.
    constexpr TaskConfig config = {.name = "clock_task", .stack_size = 512, .priority = 3};
    task_.start(config, nullptr, clock_update_step);
  }
}