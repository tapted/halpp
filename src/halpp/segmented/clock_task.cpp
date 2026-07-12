#include "halpp/segmented/clock_task.hpp"

#include <ctime>

#include "halpp/segmented/i2c_7seg.hpp"

static std::optional<uint32_t> clock_update_step(YieldingTask<ClockTask::TaskData>& task) {
  tm timeinfo;
  uint32_t delay_ms = HAL::I2C7Seg::default_instance().show_time(&timeinfo);

  ClockTask::TaskData* alarms = task.data();
  for (size_t i = 0; i < alarms->alarms_hhmmss.size(); ++i) {
    if (!alarms->on_alarm) continue;

    ClockTask::HhMmSs& t = alarms->alarms_hhmmss[i];
    if (t.state == ClockTask::State::Idle) continue;

    if (timeinfo.tm_hour == t.hour && timeinfo.tm_min == t.minute && timeinfo.tm_sec == t.second) {
      if (t.state == ClockTask::State::Active) {
        t.state = ClockTask::State::Triggered;
        alarms->on_alarm(i);
      }
    } else {
      t.state = ClockTask::State::Active;
    }
  }

  return delay_ms;
}

void ClockTask::on_time_synced() {
  // Empricially, we need 108 bytes right now. 512 is plenty if there's no logging.
  // Problem: interrupts can randomly take over the task and cause a stack overflow. Humph.
  constexpr TaskConfig config = {
      .name = "clock_task", .stack_size = 2048, .priority = 3, .use_hardware_wakeups = true};
  task_.start(config, &alarms_, clock_update_step);
}