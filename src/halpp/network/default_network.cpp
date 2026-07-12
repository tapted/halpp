#include "halpp/network/default_network.hpp"

#include <ctime>

#if __has_include("network/ssid.h")
#include "network/ssid.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "ssid_not_configured"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "password"
#endif
#ifndef TIMEZONE
#define TIMEZONE "AEST-10AEDT,M10.1.0,M4.1.0/3"
#endif

namespace {
constexpr const char TAG[] = "DefaultNetwork";
}

HAL::NtpClient::SyncCallback DefaultNetwork::time_sync_callback = nullptr;

EspResult<void> DefaultNetwork::start() {
  // Configure standard C timezone rules
  setenv("TZ", TIMEZONE, 1);
  tzset();

  // Boot LwIP Stack
  if (EspError err = network_stack_.start()) {
    return err.log(TAG, "Failed to start Net Stack");
  }

  // Initialize NTP in a dormant state
  if (EspError err = ntp_.init("pool.ntp.org", &DefaultNetwork::on_time_synced)) {
    err.log(TAG, "Failed to initialize SNTP");
    /* continue */
  }

  // Start Wi-Fi and define the cross-module triggers
  if (EspError err = wifi_.start({
          .ssid = WIFI_SSID,
          .password = WIFI_PASS,
          .on_got_ip = &DefaultNetwork::on_network_ready,
          .on_disconnected = &DefaultNetwork::on_network_lost,
          .ctx = this,
      })) {
    return err.log(TAG, "Failed to start Wi-Fi");
  }
  return ESP_OK;
}

void DefaultNetwork::network_ready(const esp_netif_ip_info_t& ip_info) {
  ESP_LOGI(TAG, "Network is ready with IP: " IPSTR, IP2STR(&ip_info.ip));
}

void DefaultNetwork::network_lost() {
  ESP_LOGW(TAG, "Network connection lost");
}

// static
void DefaultNetwork::on_time_synced(struct timeval* tv) {
  // This executes instantly when the NTP response arrives.
  // The system clock (gettimeofday) has ALREADY been updated when this fires.

  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char strftime_buf[64];
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "Time synchronized. The current time is: %s", strftime_buf);
  if (time_sync_callback) {
    time_sync_callback(tv);
  }
  // HAL::NtpClient::log_servers();
}