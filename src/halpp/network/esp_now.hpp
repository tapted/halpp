#pragma once

#include "espbase/esp_result.hpp"

typedef struct esp_now_recv_info esp_now_recv_info_t;

namespace HAL {

struct EspNowConfig {
  // If true, the ESP-NOW peer list will not be initialized with the broadcast address.
  bool skip_broadcast_peer = false;
  void (*on_data_recv)(const uint8_t* sender_mac, const uint8_t* payload, size_t len) = nullptr;
};

class EspNow {
 public:
  constexpr EspNow() = default;
  ~EspNow() { reset(); }

  EspResult<void> start(EspNowConfig config = {});
  void reset();

 private:
  EspNow(const EspNow&) = delete;
  EspNow& operator=(const EspNow&) = delete;
  EspNow(EspNow&&) = delete;
  EspNow& operator=(EspNow&&) = delete;

  static void on_data_recv_trampoline(const esp_now_recv_info_t* info, const uint8_t* incomingData,
                                      int len);
  EspNowConfig config_;
};

}  // namespace HAL