#pragma once

#include "halpp/network/net_stack.hpp"
#include "halpp/network/ntp_client.hpp"
#include "halpp/network/wifi.hpp"

class DefaultNetwork {
 public:
  static HAL::NtpClient::SyncCallback time_sync_callback;

  constexpr DefaultNetwork() = default;
  EspResult<void> start();

  virtual ~DefaultNetwork() = default;
  virtual void network_ready(const esp_netif_ip_info_t& ip_info);
  virtual void network_lost();

 private:
  HAL::NetStack network_stack_;
  HAL::NtpClient ntp_;
  HAL::WifiSta wifi_;

  static void on_network_ready(const esp_netif_ip_info_t& ip_info, void* ctx) {
    DefaultNetwork* instance = static_cast<DefaultNetwork*>(ctx);
    instance->ntp_.start();
    instance->network_ready(ip_info);
  }
  static void on_network_lost(void* ctx) { static_cast<DefaultNetwork*>(ctx)->network_lost(); }
  static void on_time_synced(struct timeval* tv);
};