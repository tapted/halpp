#pragma once

#include <cstdint>
#include <esp_event_base.h>
#include <esp_netif_types.h>
#include <string>

#include "espbase/esp_result.hpp"

namespace HAL {

// The Wi-Fi Station Config.
struct WifiConfig {
  const char* ssid = nullptr;
  const char* password = nullptr;
  bool auto_reconnect = true;
  bool ignore_stored_credentials = false;  // If true, will not use any previously stored credentials

  // Decoupled Event Callbacks
  void (*on_got_ip)(const esp_netif_ip_info_t& ip_info, void* ctx) = nullptr;
  void (*on_disconnected)(void* ctx) = nullptr;
  void* ctx = nullptr;  // Opaque pointer to the owning class
};

// The Wi-Fi Station Handle
class WifiSta {
 public:
  constexpr WifiSta() = default;
  ~WifiSta() { reset(); }

  // Returns a provisioning QR code if ssid/password are null and no creds were stored.
  EspResult<std::string> start(const WifiConfig& config);
  void reset();

  EspResult<void> disconnect();
  EspResult<void> reconnect();  // Manual trigger if auto_reconnect is false

  bool is_provisioning() const { return provisioning_; }

 private:
  WifiSta(const WifiSta&) = delete;
  WifiSta& operator=(const WifiSta&) = delete;
  WifiSta(WifiSta&&) = delete;
  WifiSta& operator=(WifiSta&&) = delete;

  esp_netif_t* netif_sta_ = nullptr;
  esp_event_handler_instance_t wifi_handler_ = nullptr;
  esp_event_handler_instance_t ip_handler_ = nullptr;

  WifiConfig config_;
  bool initialized_ = false;
  bool provisioning_ = false;

  // Static trampoline to route C-events back to the C++ object instance
  static void event_trampoline(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data);
};

}  // namespace HAL