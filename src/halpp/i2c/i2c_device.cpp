#include "halpp/i2c/i2c_device.hpp"

#include <driver/i2c_master.h>

I2CDevice& I2CDevice::operator=(I2CDevice&& other) noexcept {
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

EspResult<> I2CDevice::reset() {
  i2c_master_dev_handle_t handle = handle_;
  handle_ = nullptr;
  return handle ? i2c_master_bus_rm_device(handle) : ESP_OK;
}

EspResult<> I2CDevice::tx(std::span<const uint8_t> data, int timeout_ms) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit(handle_, data.data(), data.size(), timeout_ms);
}

EspResult<> I2CDevice::rx(std::span<uint8_t> data, int timeout_ms) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return i2c_master_receive(handle_, data.data(), data.size(), timeout_ms);
}

EspResult<> I2CDevice::txrx(std::span<const uint8_t> tx_data, std::span<uint8_t> rx_data,
                            int timeout_ms) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit_receive(handle_, tx_data.data(), tx_data.size(), rx_data.data(),
                                     rx_data.size(), timeout_ms);
}
