#pragma once

#include <driver/i2c_types.h>
#include <span>

#include "espbase/esp_result.hpp"

class I2CDevice {
 private:
  i2c_master_dev_handle_t handle_ = nullptr;

 public:
  constexpr I2CDevice() = default;
  explicit I2CDevice(i2c_master_dev_handle_t h) : handle_(h) {}

  explicit operator bool() const { return handle_ != nullptr; }
  bool operator!() const { return handle_ == nullptr; }

  // Access the raw ESP-IDF handle for driver calls
  i2c_master_dev_handle_t get() const { return handle_; }

  I2CDevice(const I2CDevice&) = delete;
  I2CDevice& operator=(const I2CDevice&) = delete;

  I2CDevice(I2CDevice&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }

  I2CDevice& operator=(I2CDevice&& other) noexcept;
  ~I2CDevice() { reset(); }

  EspResult<> reset();

  // Calls i2c_master_transmit(), or returns ESP_ERR_INVALID_STATE if !handle_;
  [[nodiscard]] EspResult<> tx(std::span<const uint8_t> data, int timeout_ms = 1000);

  // Calls i2c_master_receive(), or returns ESP_ERR_INVALID_STATE if !handle_;
  [[nodiscard]] EspResult<> rx(std::span<uint8_t> data, int timeout_ms = 1000);

  // Calls i2c_master_transmit_receive(), or returns ESP_ERR_INVALID_STATE if !handle_;
  [[nodiscard]] EspResult<> txrx(std::span<const uint8_t> tx_data, std::span<uint8_t> rx_data,
                                 int timeout_ms = 1000);

  // --- Register Helpers ---
  [[nodiscard]] EspResult<> write_reg(uint8_t reg, uint8_t data, int timeout_ms = 1000) {
    const std::array<uint8_t, 2> buf = {reg, data};
    return tx(buf, timeout_ms);
  }

  template <size_t N>
  [[nodiscard]] EspResult<> write_reg(uint8_t reg, const uint8_t (&data)[N],
                                      int timeout_ms = 1000) {
    std::array<uint8_t, N + 1> write_buf;
    write_buf[0] = reg;
    memcpy(write_buf.data() + 1, data, N);
    return tx(write_buf, timeout_ms);
  }

  [[nodiscard]] EspResult<uint8_t> read_reg(uint8_t reg, int timeout_ms = 1000) {
    uint8_t rx_buf = 0;
    const std::array<uint8_t, 1> tx_buf = {reg};
    if (EspError err = txrx(tx_buf, std::span<uint8_t>(&rx_buf, 1), timeout_ms)) return err;
    return rx_buf;
  }

  template <size_t N>
  [[nodiscard]] EspResult<> read_reg(uint8_t reg, uint8_t (&data)[N], int timeout_ms = 1000) {
    const std::array<uint8_t, 1> tx_buf = {reg};
    return txrx(tx_buf, data, timeout_ms);
  }

  [[nodiscard]] EspResult<uint8_t> read_byte(int timeout_ms = 1000) {
    uint8_t rx_buf = 0;
    if (EspError err = rx(std::span<uint8_t>(&rx_buf, 1), timeout_ms)) return err;
    return rx_buf;
  }
};