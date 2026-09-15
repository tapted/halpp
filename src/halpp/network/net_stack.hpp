#pragma once

#include "espbase/esp_result.hpp"

namespace halpp {

// Initializes LwIP and the Default Event Loop. Must exist before Wi-Fi or MQTT.
class NetStack {
 public:
  constexpr NetStack() = default;
  ~NetStack() { reset(); }

  NetStack(const NetStack&) = delete;
  NetStack& operator=(const NetStack&) = delete;
  NetStack(NetStack&&) = delete;
  NetStack& operator=(NetStack&&) = delete;

  EspResult<> start();
  void reset();

 private:
  bool initialized_ = false;
};

}  // namespace halpp