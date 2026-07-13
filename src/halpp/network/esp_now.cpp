#include "halpp/network/esp_now.hpp"

#include <esp_now.h>
#include <esp_wifi.h>

namespace HAL {

static constexpr const char TAG[] = "HALPP_ESP_NOW";
static EspNow* instance = nullptr;

EspResult<void> EspNow::start(EspNowConfig config) {
  if (instance) return ESP_ERR_INVALID_STATE;
  config_ = config;

  // 3. Initialize the Wi-Fi Hardware
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (EspError err = esp_wifi_init(&cfg)) return err.log(TAG, "esp_wifi_init");

  // 4. Set Storage and Mode
  // Use RAM storage so it doesn't burn out your flash memory writing states,
  // and set it to Station mode (standard for ESP-NOW).
  if (EspError err = esp_wifi_set_storage(WIFI_STORAGE_RAM))
    return err.log(TAG, "esp_wifi_set_storage");
  if (EspError err = esp_wifi_set_mode(WIFI_MODE_STA)) return err.log(TAG, "esp_wifi_set_mode");

  // 5. START the Wi-Fi Radio
  if (EspError err = esp_wifi_start()) return err.log(TAG, "esp_wifi_start");

  // 6. FINALLY, initialize ESP-NOW
  if (EspError err = esp_now_init()) {
    return err.log(TAG, "esp_now_init");
  }

  if (config_.on_data_recv) {
    if (EspError err = esp_now_register_recv_cb(&on_data_recv_trampoline))
      return err.log(TAG, "esp_now_register_recv_cb");
  }

  if (!config_.skip_broadcast_peer) {
    esp_now_peer_info_t peerInfo = {};
    constexpr uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (EspError err = esp_now_add_peer(&peerInfo)) {
      err.log(TAG, "Failed to add broadcast peer");
    }
  }

  ESP_LOGI(TAG, "ESP-NOW Initialized Successfully!");

  instance = this;
  return ESP_OK;
}

void EspNow::reset() {
  if (instance) {
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    instance = nullptr;
  }
}

void EspNow::on_data_recv_trampoline(const esp_now_recv_info_t* info, const uint8_t* incomingData,
                                     int len) {
  if (instance && instance->config_.on_data_recv) {
    instance->config_.on_data_recv(info->src_addr, incomingData, static_cast<size_t>(len));
  }
}
}  // namespace HAL