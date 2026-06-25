#pragma once

#include "espbase/yielding_task.hpp"

class ClockTask {
 public:
  constexpr ClockTask() = default;
  void on_time_synced();
  YieldingTask<int> task_;
};
