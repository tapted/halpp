#include "halpp/segmented/clock_task.hpp"

#include <ctime>

#include "halpp/segmented/i2c_7seg.hpp"

static std::optional<uint32_t> clock_update_step(MainLoopTask<ClockTask::TaskData>& task) {
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
  // Ignore return (means task is already running).
  task_.start({.name = "clock_task"}, &alarms_, clock_update_step);
}