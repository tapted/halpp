#include "halpp/network/wifi.hpp"

#include <esp_event.h>
#include <esp_wifi.h>
#include <string.h>

namespace HAL {

static const char* TAG = "WIFI_HAL";

EspResult<void> WifiSta::start(const WifiConfig& config) {
  if (initialized_) return ESP_ERR_INVALID_STATE;
  config_ = config;

  // 1. Create the Station Netif
  netif_sta_ = esp_netif_create_default_wifi_sta();
  if (!netif_sta_) return ESP_ERR_NO_MEM;

  // 2. Init the Driver
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_err_t err = esp_wifi_init(&cfg)) {
    esp_netif_destroy(netif_sta_);
    netif_sta_ = nullptr;
    return err;
  }

  // 3. Register Event Handlers, passing 'this' as the argument
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_trampoline, this,
                                      &wifi_handler_);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_trampoline, this,
                                      &ip_handler_);

  // 4. Configure Credentials
  wifi_config_t wifi_config = {};
  if (config_.ssid)
    strlcpy((char*)wifi_config.sta.ssid, config_.ssid, sizeof(wifi_config.sta.ssid));
  if (config_.password)
    strlcpy((char*)wifi_config.sta.password, config_.password, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

  // 5. Start the Radio (this triggers WIFI_EVENT_STA_START asynchronously)
  if (esp_err_t err = esp_wifi_start()) {
    reset();  // Graceful rollback
    return err;
  }

  initialized_ = true;
  return ESP_OK;
}

void WifiSta::reset() {
  if (initialized_) {
    // 1. Deafen the events immediately to prevent auto-reconnect loops
    if (wifi_handler_)
      esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_);
    if (ip_handler_)
      esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_handler_);
    wifi_handler_ = nullptr;
    ip_handler_ = nullptr;

    // 2. Shut down hardware
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    if (netif_sta_) {
      esp_netif_destroy(netif_sta_);
      netif_sta_ = nullptr;
    }
    initialized_ = false;
  }
}

EspResult<void> WifiSta::disconnect() {
  if (!initialized_) return ESP_ERR_INVALID_STATE;
  return esp_wifi_disconnect();
}

EspResult<void> WifiSta::reconnect() {
  if (!initialized_) return ESP_ERR_INVALID_STATE;
  return esp_wifi_connect();
}

void WifiSta::event_trampoline(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data) {
  WifiSta* instance = static_cast<WifiSta*>(arg);

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "Driver started. Connecting...");
    esp_wifi_connect();

  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "Disconnected from AP.");

    if (instance->config_.on_disconnected) {
      instance->config_.on_disconnected(instance->config_.ctx);
    }
    if (instance->config_.auto_reconnect) {
      ESP_LOGI(TAG, "Auto-reconnecting...");
      esp_wifi_connect();
    }

  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    if (instance->config_.on_got_ip) {
      instance->config_.on_got_ip(event->ip_info, instance->config_.ctx);
    }
  }
}

}  // namespace HAL