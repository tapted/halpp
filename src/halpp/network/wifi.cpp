#include "halpp/network/wifi.hpp"

#include <esp_dpp.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <string.h>
#include <string>

#include "espbase/nvs_store.hpp"

namespace HAL {

static constexpr const char TAG[] = "WIFI_HAL";
static constexpr const char NAMESPACE[] = "HALPP_WIFI";
static constexpr const char SSID_KEY[] = "wifi_ssid";
static constexpr const char PASS_KEY[] = "wifi_pass";
static constinit std::string dpp_uri;
static SemaphoreHandle_t uri_ready_sem_ = nullptr;

namespace {

// https://github.com/espressif/esp-idf/blob/v6.0.2/examples/wifi/wifi_easy_connect/dpp-enrollee/main/dpp_enrollee_main.c
EspResult<std::string> start_dpp_provisioning(const std::string& listen_channel) {
  uri_ready_sem_ = xSemaphoreCreateBinary();
  if (!uri_ready_sem_) return ESP_ERR_NO_MEM;

  if (EspError err = esp_supp_dpp_init()) {
    return err.log(TAG, "Failed to initialize DPP supplicant");
  }

  // Generates Bootstrap Information. This asynchronously posts WIFI_EVENT_DPP_URI_READY
  if (EspError err = esp_supp_dpp_bootstrap_gen(listen_channel.c_str(), DPP_BOOTSTRAP_QR_CODE,
                                                nullptr, nullptr)) {
    return err.log(TAG, "Failed to generate DPP bootstrap info");
  }

  ESP_LOGI(TAG, "DPP provisioning initialized. Waiting for URI generation...");

  // Block the main task until the event handler catches the URI and gives the semaphore
  if (xSemaphoreTake(uri_ready_sem_, pdMS_TO_TICKS(5000)) != pdTRUE) {
    return EspError(ESP_ERR_TIMEOUT).log(TAG, "Timed out waiting for DPP URI");
  }

  vSemaphoreDelete(uri_ready_sem_);
  uri_ready_sem_ = nullptr;

  ESP_LOGI(TAG, "DPP URI Generated! [%s]. Starting Wifi.", dpp_uri.c_str());
  if (EspError err = esp_wifi_start()) {
    err.log(TAG, "Failed to start Wi-Fi radio for DPP.");
    // Continue? Maybe something else started it already.
  }

  return std::move(dpp_uri);
}
}  // namespace

EspResult<std::string> WifiSta::start(const WifiConfig& config) {
  if (initialized_) return ESP_ERR_INVALID_STATE;
  config_ = config;

  // 1. Create the Station Netif
  netif_sta_ = esp_netif_create_default_wifi_sta();
  if (!netif_sta_) return ESP_ERR_NO_MEM;

  // 2. Init the Driver
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_err_t err = esp_wifi_init(&cfg)) {
    if (err == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "esp_wifi_init() gave ESP_ERR_INVALID_STATE. Stopping wifi. Assuming inited.");
      esp_wifi_stop();  // Stop to ensure the netif sees the connection events.
    } else {
      esp_netif_destroy(netif_sta_);
      netif_sta_ = nullptr;
      return err;
    }
  }

  // 3. Register Event Handlers, passing 'this' as the argument
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_trampoline, this,
                                      &wifi_handler_);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_trampoline, this,
                                      &ip_handler_);

  esp_wifi_set_mode(WIFI_MODE_STA);

  // 4. Configure Credentials
  wifi_config_t wifi_config = {};
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  if (config_.ssid && config_.password) {
    // Explicit config passed.
    strlcpy((char*)wifi_config.sta.ssid, config_.ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, config_.password, sizeof(wifi_config.sta.password));
  } else if (!config_.ignore_stored_credentials) {
    auto store = NvsStore::open(NAMESPACE);
    if (store &&
        store->get_string(SSID_KEY, (char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid)) &&
        store->get_string(PASS_KEY, (char*)wifi_config.sta.password,
                          sizeof(wifi_config.sta.password))) {
      ESP_LOGI(TAG, "Loaded Wi-Fi credentials from NVS: SSID=%s", wifi_config.sta.ssid);
    }
  }

  if (wifi_config.sta.ssid[0] == '\0') {
    ESP_LOGI(TAG, "No Wi-Fi credentials provided or stored. Starting DPP provisioning...");
    provisioning_ = true;
    initialized_ = true;
    return start_dpp_provisioning("6");
  }

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
    if (instance->is_provisioning()) {
      ESP_LOGI(TAG, "Driver started. Beginning DPP listening phase.");
      esp_supp_dpp_start_listen();
    } else {
      ESP_LOGI(TAG, "Driver started. Connecting...");
      esp_wifi_connect();
    }

  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_DPP_URI_READY && event_data) {
    wifi_event_dpp_uri_ready_t* uri_ready = static_cast<wifi_event_dpp_uri_ready_t*>(event_data);
    dpp_uri = std::string(uri_ready->uri, uri_ready->uri_data_len);
    if (uri_ready_sem_) xSemaphoreGive(uri_ready_sem_);

  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_DPP_CFG_RECVD) {
    ESP_LOGI(TAG, "DPP Configuration received from Android!");
    auto* cfg_received = static_cast<wifi_event_dpp_config_received_t*>(event_data);
    esp_wifi_set_config(WIFI_IF_STA, &cfg_received->wifi_cfg);
    esp_wifi_connect();

  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_DPP_FAILED) {
    wifi_event_dpp_failed_t* dpp_failed = static_cast<wifi_event_dpp_failed_t*>(event_data);
    ESP_LOGE(TAG, "DPP Authentication Failed. Reason: 0x%x (%s). Restarting listen...",
             dpp_failed->failure_reason, esp_err_to_name(dpp_failed->failure_reason));
    esp_supp_dpp_start_listen();

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

    wifi_config_t conf;
    if (instance->is_provisioning() && esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
      size_t ssid_len = strnlen(reinterpret_cast<char*>(conf.sta.ssid), sizeof(conf.sta.ssid));
      size_t pass_len =
          strnlen(reinterpret_cast<char*>(conf.sta.password), sizeof(conf.sta.password));

      std::string ssid(reinterpret_cast<char*>(conf.sta.ssid), ssid_len);
      std::string password(reinterpret_cast<char*>(conf.sta.password), pass_len);

      ESP_LOGI(TAG, "Captured credentials for SSID: %s", ssid.c_str());

      auto store = NvsStore::open(NAMESPACE);
      if (store) {
        store->set_string(SSID_KEY, ssid.c_str()).log_error(TAG, "set_ssid");
        store->set_string(PASS_KEY, password.c_str()).log_error(TAG, "set_pass");
        store->commit().log_error(TAG, "commit ssid/pass");
      } else {
        store.strip().log_error(TAG, "Failed to open NVS store for saving Wi-Fi credentials");
      }
      // Provisioning is complete. Cleanly kill the DPP stack.
      esp_supp_dpp_stop_listen();
      esp_supp_dpp_deinit();
    }
  }
}

}  // namespace HAL