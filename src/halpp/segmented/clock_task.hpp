#pragma once

#include <array>

#include "espbase/yielding_task.hpp"

class ClockTask {
 public:
  using OnAlarm = void (*)(size_t index);
  static constexpr size_t MAX_ALARMS = 32;
  enum class State : uint8_t { Idle, Active, Triggered };
  struct HhMmSs {
    State state = State::Idle;
    uint8_t hour = 7;
    uint8_t minute = 0;
    uint8_t second = 0;
  };

  struct TaskData {
    std::array<HhMmSs, MAX_ALARMS> alarms_hhmmss{};
    OnAlarm on_alarm = nullptr;
  };

  constexpr ClockTask(OnAlarm on_alarm = nullptr) { alarms_.on_alarm = on_alarm; }
  void on_time_synced();

  YieldingTask<TaskData> task_;

  void set_alarm(size_t index, uint8_t hour, uint8_t minute, uint8_t second) {
    if (index >= MAX_ALARMS) return;
    alarms_.alarms_hhmmss[index] = HhMmSs{State::Active, hour, minute, second};
  }

 private:
  TaskData alarms_;
};
