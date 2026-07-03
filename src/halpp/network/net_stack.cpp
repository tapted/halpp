#include "halpp/network/net_stack.hpp"

#include <esp_event.h>
#include <esp_netif.h>

namespace {
constexpr const char TAG[] = "NetStack";
}

namespace HAL {

EspResult<void> NetStack::start() {
  if (initialized_) return ESP_ERR_INVALID_STATE;
  if (EspError err = esp_netif_init()) return err.log(TAG, "esp_netif_init");
  if (EspError err = esp_event_loop_create_default())
    return err.log(TAG, "esp_event_loop_create_default");
  initialized_ = true;
  return ESP_OK;
}

void NetStack::reset() {
  if (initialized_) {
    esp_event_loop_delete_default();
    esp_netif_deinit();
    initialized_ = false;
  }
}

}  // namespace HAL