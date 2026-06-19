#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <stdint.h>

#include "espbase/esp_result.hpp"

#include "halpp/i2c/i2c_device.hpp"

// ============================================================================
// I2C Master Driver Class
// ============================================================================
class I2CMaster {
  static bool _initialized;
  esp_err_t _deinit();

 public:
  static I2CMaster& instance() {
    static I2CMaster inst;
    return inst;
  }
  static esp_err_t deinit() { return _initialized ? instance()._deinit() : ESP_OK; }

  // Initialization
  esp_err_t init(bool scan_devices = false);
  bool is_initialized() const { return _initialized; }

  // Device management (for creating device handles)
  EspResult<I2CDevice> add_device(uint8_t device_addr, uint32_t scl_speed_hz = 0,
                                  i2c_device_config_t* dev_config = nullptr);

  // Get bus handle (for advanced usage)
  i2c_master_bus_handle_t get_bus_handle() { return _bus_handle; }

  // Utility functions
  esp_err_t reset();
  esp_err_t probe(uint8_t device_addr);
  esp_err_t scan(uint8_t* found_devices, size_t* count, size_t max_count) const;
  esp_err_t wait_all_done(uint32_t timeout_ms);

 private:
  I2CMaster();
  ~I2CMaster();

  i2c_master_bus_handle_t _bus_handle = nullptr;
};
