#include "halpp/segmented/clock_task.hpp"

#include "halpp/segmented/i2c_7seg.hpp"

static std::optional<uint32_t> clock_update_step(YieldingTask<int>&) {
  return HAL::I2C7Seg::default_instance().show_time();
}

void ClockTask::on_time_synced() {
  if (task_.running()) {
    task_.notify();
  } else {
    constexpr TaskConfig config = {.name = "clock_task", .stack_size = 1024, .priority = 3};
    task_.start(config, nullptr, clock_update_step);
  }
}