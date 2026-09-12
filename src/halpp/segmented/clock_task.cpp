#include "halpp/segmented/clock_task.hpp"

#include <ctime>

#include "halpp/segmented/i2c_7seg.hpp"

static std::optional<uint32_t> clock_update_step(MainLoopTask<ClockTask::TaskData>& task) {
  tm timeinfo;
  uint32_t delay_ms = HAL::I2C7Seg::default_instance().show_time(&timeinfo);

  // Convert current time to seconds since midnight
  uint32_t current_sec_of_day =
      (timeinfo.tm_hour * 3600) + (timeinfo.tm_min * 60) + timeinfo.tm_sec;

  ClockTask::TaskData* alarms = task.data();
  for (size_t i = 0; i < alarms->alarms_hhmmss.size(); ++i) {
    if (!alarms->on_alarm) continue;

    ClockTask::HhMmSs& t = alarms->alarms_hhmmss[i];
    if (t.state == ClockTask::State::Idle) continue;
    
    if ((t.day_mask & (1 << timeinfo.tm_wday)) == 0) continue;

    // Convert alarm time to seconds since midnight
    uint32_t alarm_sec_of_day = (t.hour * 3600) + (t.minute * 60) + t.second;

    // Check if we are within a 5-second safety window of the alarm
    bool is_alarm_time =
        (current_sec_of_day >= alarm_sec_of_day) && (current_sec_of_day < alarm_sec_of_day + 5);

    if (is_alarm_time) {
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