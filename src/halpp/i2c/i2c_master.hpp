#pragma once

#include <cstdint>
#include <esp_err.h>

#include "espbase/esp_result.hpp"
#include "halpp/i2c/i2c_device.hpp"

#include <hal/i2c_types.h>

// ============================================================================
// I2C Master Driver Class
// ============================================================================
class I2CMaster {
  static bool _initialized;  // Guards static deinit() to avoid creating the singleton.
  esp_err_t _deinit();

 public:
  static I2CMaster& instance() {
    static I2CMaster inst;
    return inst;
  }
  static esp_err_t deinit() { return _initialized ? instance()._deinit() : ESP_OK; }

  // Device management (for creating device handles)
  EspResult<I2CDevice> add_device(uint8_t device_addr, uint32_t scl_speed_hz = 0,
                                  uint32_t scl_wait_us = 0,
                                  i2c_addr_bit_len_t dev_addr_length = I2C_ADDR_BIT_LEN_7);

  // Get bus handle (for advanced usage)
  i2c_master_bus_handle_t get_bus_handle() { return _bus_handle; }

  // Utility functions
  esp_err_t reset();
  esp_err_t probe(uint8_t device_addr);
  esp_err_t scan(uint8_t* found_devices, size_t* count, size_t max_count) const;
  esp_err_t wait_all_done(uint32_t timeout_ms);

 private:
  I2CMaster();
  ~I2CMaster();  // Never invoked.

  i2c_master_bus_handle_t _bus_handle = nullptr;
};
