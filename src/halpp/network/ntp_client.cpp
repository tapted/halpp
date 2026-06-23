#include "halpp/network/ntp_client.hpp"

#include <esp_netif_sntp.h>
#include <esp_sntp.h>

namespace HAL {

EspResult<void> NtpClient::init(const char* fallback_server, NtpClient::SyncCallback on_sync) {
  if (initialized_) return ESP_ERR_INVALID_STATE;

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(fallback_server);

  // Prevent the service from turning on immediately
  config.start = false;

  // Explicitly allow accepting options offered by DHCP
  config.server_from_dhcp = true;

  if (on_sync) {
    config.sync_cb = on_sync;
  }

  if (esp_err_t err = esp_netif_sntp_init(&config)) {
    return err;
  }

  initialized_ = true;
  return ESP_OK;
}

void NtpClient::start() {
  if (initialized_) {
    esp_netif_sntp_start();
  }
}

void NtpClient::reset() {
  if (initialized_) {
    esp_netif_sntp_deinit();
    initialized_ = false;
  }
}

// static
void NtpClient::log_servers() {
  // By default, ESP-IDF supports 1 to 3 simultaneous NTP servers
  uint8_t max_servers = 3;// esp_sntp_get_max_sync_servers();

  ESP_LOGI("NTP", "Active SNTP Servers:");
  for (uint8_t i = 0; i < max_servers; ++i) {
    const char* name = esp_sntp_getservername(i);
    const ip_addr_t* ip = esp_sntp_getserver(i);

    if (name) {
      ESP_LOGI("NTP", "  [%d] Hostname: %s (Configured Fallback)", i, name);
    } else if (ip && !ip_addr_isany(ip)) {
      // ipaddr_ntoa safely converts the lwIP IP struct to a human-readable string
      ESP_LOGI("NTP", "  [%d] IP Address: %s (Provided by DHCP)", i, ipaddr_ntoa(ip));
    } else {
      ESP_LOGI("NTP", "  [%d] <Empty slot>", i);
      break;
    }
  }
}

}  // namespace HAL