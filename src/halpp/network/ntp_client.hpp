/**
 * @file ntp.hpp
 * @brief RAII wrapper for ESP-IDF SNTP Time Synchronization
 */

#pragma once

#include "espbase/esp_result.hpp"

struct timeval;

namespace HAL {

class NtpClient {
 public:
  using SyncCallback = void (*)(struct timeval* tv);

  constexpr NtpClient() = default;
  ~NtpClient() { reset(); }

  // Must remain pinned in memory
  NtpClient(const NtpClient&) = delete;
  NtpClient& operator=(const NtpClient&) = delete;
  NtpClient(NtpClient&&) = delete;
  NtpClient& operator=(NtpClient&&) = delete;

  EspResult<void> init(const char* fallback_server = "pool.ntp.org",
                       SyncCallback on_sync = nullptr);

  // Triggers the synchronization. Must only be called after acquiring a DHCP lease.
  void start();
  void reset();
  static void log_servers();

 private:
  bool initialized_ = false;
};

}  // namespace HAL