#pragma once

#include "halpp/network/net_stack.hpp"
#include "halpp/network/wifi.hpp"

class DefaultNetwork {
 public:
  DefaultNetwork();
  virtual ~DefaultNetwork() = default;
  virtual void network_ready(const esp_netif_ip_info_t& ip_info);
  virtual void network_lost();

 private:
  HAL::NetStack network_stack_;
  HAL::WifiSta wifi_;

  static void on_network_ready(const esp_netif_ip_info_t& ip_info, void* ctx) {
    static_cast<DefaultNetwork*>(ctx)->network_ready(ip_info);
  }
  static void on_network_lost(void* ctx) {
    static_cast<DefaultNetwork*>(ctx)->network_lost();
  }
};