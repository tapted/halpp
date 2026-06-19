#pragma once

#include <driver/i2c_master.h>

#include "espbase/esp_result.hpp"

class I2CDevice {
 private:
  i2c_master_dev_handle_t handle_ = nullptr;

 public:
  constexpr I2CDevice() = default;
  explicit I2CDevice(i2c_master_dev_handle_t h) : handle_(h) {}

  explicit operator bool() const { return handle_ != nullptr; }
  bool operator!() const { return handle_ == nullptr; }

  I2CDevice(const I2CDevice&) = delete;
  I2CDevice& operator=(const I2CDevice&) = delete;

  I2CDevice(I2CDevice&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }

  I2CDevice& operator=(I2CDevice&& other) noexcept {
    if (this != &other) {
      // If we already own a device, clean it up before taking the new one
      if (handle_) {
        i2c_master_bus_rm_device(handle_);
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~I2CDevice() { reset(); }

  EspResult<void> reset() {
    i2c_master_dev_handle_t handle = handle_;
    handle_ = nullptr;
    return handle ? i2c_master_bus_rm_device(handle) : ESP_OK;
  }

  template <size_t N>
  EspResult<void> tx(const uint8_t (&data)[N], int timeout_ms = 1000) {
    if (!handle_) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit(handle_, data, N, timeout_ms);
  }

  template <size_t TN, size_t RN>
  EspResult<void> txrx(const uint8_t (&data)[TN], uint8_t (&rx_data)[RN], int timeout_ms = 1000) {
    if (!handle_) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(handle_, data, TN, rx_data, RN, timeout_ms);
  }

  template <size_t N>
  EspResult<void> write_reg(uint8_t reg, const uint8_t (&data)[N], int timeout_ms = 1000) {
    uint8_t write_buf[N + 1];
    write_buf[0] = reg;
    memcpy(write_buf + 1, data, N);
    return tx(write_buf, timeout_ms);
  }

  EspResult<void> write_reg(uint8_t reg, uint8_t data, int timeout_ms = 1000) {
    return tx({reg, data}, timeout_ms);
  }

  template <size_t N>
  EspResult<void> read_reg(uint8_t reg, uint8_t (&data)[N], int timeout_ms = 1000) {
    return txrx({reg}, data, timeout_ms);
  }

  EspResult<uint8_t> read_reg(uint8_t reg, int timeout_ms = 1000) {
    uint8_t rx_buf[1] = {0};
    esp_err_t err = txrx({reg}, rx_buf, timeout_ms).error();
    if (err != ESP_OK) return err;
    return rx_buf[0];
  }

  // Access the raw ESP-IDF handle for driver calls
  i2c_master_dev_handle_t get() const { return handle_; }
};