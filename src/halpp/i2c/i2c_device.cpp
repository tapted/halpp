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

EspResult<void> I2CDevice::reset() {
  i2c_master_dev_handle_t handle = handle_;
  handle_ = nullptr;
  return handle ? i2c_master_bus_rm_device(handle) : ESP_OK;
}

EspResult<void> I2CDevice::transmit(const uint8_t* data, size_t length, int timeout_ms) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit(handle_, data, length, timeout_ms);
}

EspResult<void> I2CDevice::transmit_receive(const uint8_t* tx_data, size_t tx_length,
                                            uint8_t* rx_data, size_t rx_length, int timeout_ms) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit_receive(handle_, tx_data, tx_length, rx_data, rx_length, timeout_ms);
}
