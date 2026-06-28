#pragma once

#include <vector>

#include "espbase/yielding_task.hpp"


class ClockTask {
 public:
  using OnAlarm = void (*)(size_t index);
  struct TaskData {
    std::vector<uint32_t> alarms_hhmmss;
    OnAlarm on_alarm = nullptr;
  };

  constexpr ClockTask(OnAlarm on_alarm = nullptr) {
    alarms_.on_alarm = on_alarm;
  }
  void on_time_synced();

  YieldingTask<TaskData> task_;

  void set_alarm(size_t index, uint8_t hour, uint8_t minute, uint8_t second) {
    if (index >= alarms_.alarms_hhmmss.size()) {
      alarms_.alarms_hhmmss.resize(index + 1);
    }
    alarms_.alarms_hhmmss[index] = 1 << 24 | (hour << 16) | (minute << 8) | second;
  }

 private:
  TaskData alarms_;
};
