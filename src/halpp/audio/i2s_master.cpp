#include "halpp/audio/i2s_master.hpp"

#include <driver/i2s_std.h>
#include <esp_log.h>

#include "halpp/config.hpp"

static constexpr const char TAG[] = "I2S_MASTER";
namespace halpp::audio {

// Generates the unified config required by Full-Duplex
static i2s_std_config_t get_std_cfg() {
  i2s_std_config_t cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(HAL::config::Audio::SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(HAL::config::Audio::BITS_PER_SAMPLE,
                                                      HAL::config::Audio::SLOT_MODE),
      .gpio_cfg =
          {
              .mclk = HAL::config::Audio::PIN_MCLK,
              .bclk = HAL::config::Audio::PIN_BCK,
              .ws = HAL::config::Audio::PIN_WS,
              .dout = HAL::config::Audio::PIN_DATA_OUT,
              .din = HAL::config::Audio::PIN_DATA_IN,
              .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
          },
  };
  // The default on esp32s3 is SOC_MOD_CLK_PLL_F160M. I2S_CLK_SRC_PLL_240M exists, but maybe is no
  // better.
  // cfg.clk_cfg.clk_src = I2S_CLK_SRC_PLL_240M;

  // I2S_MCLK_MULTIPLE_256 is default and "preferred" for these chips. I2S_MCLK_MULTIPLE_384 is
  // recommended for 24-bit samples. Maybe it works better at frequencies other than 48kHz? Worth
  // testing.
  // cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
  return cfg;
}

I2SMaster::I2SMaster() {
  ESP_LOGI(TAG, "I2SMaster constructor");
}

I2SMaster::~I2SMaster() {
}

EspResult<> I2SMaster::retain_tx(i2s_chan_handle_t* out_handle) {
  std::lock_guard<std::mutex> lock(_mutex);
  maybe_allocate_i2s();

  if (!_tx_initialized) {
    // If Mic is currently running, briefly disable it to sync hardware clocks safely
    bool rx_was_running = (_rx_refs > 0);
    if (rx_was_running) i2s_channel_disable(_rx_handle);

    i2s_std_config_t cfg = get_std_cfg();

    // Is this needed for stereo -> mono downmix?
    // cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    if (EspError err = i2s_channel_init_std_mode(_tx_handle, &cfg)) {
      maybe_deallocate_i2s();
      return err.log(TAG, "Failed to initialize I2S TX channel");
    }
    _tx_initialized = true;

    if (rx_was_running) i2s_channel_enable(_rx_handle);
  }

  _tx_refs++;
  if (_tx_refs == 1) i2s_channel_enable(_tx_handle);
  *out_handle = _tx_handle;
  return ESP_OK;
}

EspResult<> I2SMaster::retain_rx(i2s_chan_handle_t* out_handle) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (EspError err = maybe_allocate_i2s()) return err;

  if (!_rx_initialized) {
    // If Speaker is currently running, briefly disable it to sync hardware clocks safely
    bool tx_was_running = (_tx_refs > 0);
    if (tx_was_running) i2s_channel_disable(_tx_handle);

    i2s_std_config_t cfg = get_std_cfg();
    if (EspError err = i2s_channel_init_std_mode(_rx_handle, &cfg)) {
      maybe_deallocate_i2s();
      return err.log(TAG, "Failed to initialize I2S RX channel");
    }
    _rx_initialized = true;

    if (tx_was_running) i2s_channel_enable(_tx_handle);
  }

  _rx_refs++;
  if (_rx_refs == 1) i2s_channel_enable(_rx_handle);
  *out_handle = _rx_handle;
  return ESP_OK;
}

void I2SMaster::release_tx() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_tx_refs > 0) {
    _tx_refs--;
    if (_tx_refs == 0) i2s_channel_disable(_tx_handle);
  }
  maybe_deallocate_i2s();
}

void I2SMaster::release_rx() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_rx_refs > 0) {
    _rx_refs--;
    if (_rx_refs == 0) i2s_channel_disable(_rx_handle);
  }
  maybe_deallocate_i2s();
}

EspResult<> I2SMaster::maybe_allocate_i2s() {
  if (!_i2s_allocated) {
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(HAL::config::Audio::I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    if (EspError err = i2s_new_channel(&chan_cfg, &_tx_handle, &_rx_handle)) {
      return err.log(TAG, "Failed to allocate I2S channel");
    }
    _i2s_allocated = true;
  }
  return ESP_OK;
}

void I2SMaster::maybe_deallocate_i2s() {
  if (_tx_refs != 0 || _rx_refs != 0) return;

  if (_i2s_allocated) {
    ESP_LOGI(TAG, "All audio users released. Tearing down I2S.");
    i2s_del_channel(_tx_handle);
    i2s_del_channel(_rx_handle);
    _i2s_allocated = false;
    _tx_initialized = false;
    _rx_initialized = false;
    _tx_handle = nullptr;
    _rx_handle = nullptr;
  }
}

}  // namespace halpp::audio