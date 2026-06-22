#include "halpp/network/default_network.hpp"

#if __has_include("network/ssid.h")
#include "network/ssid.h"
#define HAS_LOCAL_SECRETS 1
#else
#define HAS_LOCAL_SECRETS 0
#endif

#if !HAS_LOCAL_SECRETS
#define WIFI_SSID "ssid_not_configured"
#define WIFI_PASS "password"
#endif

namespace {
constexpr const char TAG[] = "DefaultNetwork";
}

DefaultNetwork::DefaultNetwork() {
  // 1. Boot LwIP Stack
  if (EspError err = network_stack_.start()) {
    err.log(TAG, "Failed to start Net Stack");
    return;
  }

  // 2. Start Wi-Fi and define the cross-module triggers
  if (EspError err = wifi_.start({
          .ssid = WIFI_SSID,
          .password = WIFI_PASS,
          .on_got_ip = &DefaultNetwork::on_network_ready,
          .on_disconnected = &DefaultNetwork::on_network_lost,
          .ctx = this,
      })) {
    err.log(TAG, "Failed to start Wi-Fi");
    return;
  }
}

void DefaultNetwork::network_ready(const esp_netif_ip_info_t& ip_info) {
  ESP_LOGI(TAG, "Network is ready with IP: " IPSTR, IP2STR(&ip_info.ip));
}

void DefaultNetwork::network_lost() {
  ESP_LOGW(TAG, "Network connection lost");
}