#include "halpp/segmented/clock_task.hpp"

#include <ctime>

#include "halpp/segmented/i2c_7seg.hpp"

static std::optional<uint32_t> clock_update_step(YieldingTask<ClockTask::TaskData>& task) {
  tm timeinfo;
  uint32_t delay_ms = HAL::I2C7Seg::default_instance().show_time(&timeinfo);

  ClockTask::TaskData* alarms = task.data();
  for (size_t i = 0; i < alarms->alarms_hhmmss.size(); ++i) {
    if (!alarms->on_alarm) continue;

    uint32_t alarm_hhmmss = alarms->alarms_hhmmss[i];
    uint8_t enabled = (alarm_hhmmss >> 24) & 0x01;
    if (!enabled) continue;

    uint8_t hour = (alarm_hhmmss >> 16) & 0xFF;
    uint8_t minute = (alarm_hhmmss >> 8) & 0xFF;
    uint8_t second = alarm_hhmmss & 0xFF;
    if (timeinfo.tm_hour == hour && timeinfo.tm_min == minute && timeinfo.tm_sec == second) {
      alarms->on_alarm(i);
    }
  }

  return delay_ms;
}

void ClockTask::on_time_synced() {
  // Empricially, we need 108 bytes right now. 512 is plenty if there's no logging.
  // Problem: interrupts can randomly take over the task and cause a stack overflow. Humph.
  constexpr TaskConfig config = {.name = "clock_task", .stack_size = 2048, .priority = 3};
  task_.start(config, &alarms_, clock_update_step);
}