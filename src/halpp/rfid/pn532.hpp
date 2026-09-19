#pragma once

#include <array>
#include <cstdint>
#include <soc/gpio_num.h>
#include <span>

#include "espbase/esp_result.hpp"
#include "halpp/core/default_instance.hpp"
#include "halpp/i2c/i2c_device.hpp"

namespace halpp {

class Pn532 : public DefaultInstance<Pn532> {
 public:
  static constexpr uint8_t I2C_ADDRESS_DEFAULT = 0x24;
  using TagCallback = void (*)(void* ctx, Pn532& pn532, std::span<const uint8_t> uid);

  explicit Pn532(I2CDevice device = I2CDevice{}, gpio_num_t irq_pin = GPIO_NUM_NC)
      : i2c_dev_(std::move(device)), irq_pin_(irq_pin), scanning_(false) {}

  ~Pn532();

  static EspResult<> init_default(uint8_t i2c_address = I2C_ADDRESS_DEFAULT,
                                  gpio_num_t irq_pin = GPIO_NUM_NC);

  EspResult<> begin();
  bool is_initialized() const { return !!i2c_dev_; }

  void set_on_tag_callback(TagCallback cb, void* ctx = nullptr) {
    on_tag_cb_ = cb;
    on_tag_ctx_ = ctx;
  }

  // Starts the scan. Returns immediately after the ACK.
  EspResult<> start_passive_target_read();

  EspResult<> power_down();
  EspResult<> wake_up();
  EspResult<> sam_config();
  EspResult<> get_firmware_version(std::array<uint8_t, 4>& version_out);
  void poll() { process_tag_response(true); }

 private:
  I2CDevice i2c_dev_;
  gpio_num_t irq_pin_;
  void* on_tag_ctx_;
  TagCallback on_tag_cb_;
  volatile bool scanning_;  // Volatile as it is checked/modified near ISR boundaries

  // ISR and Main Loop Thunks
  static void IRAM_ATTR gpio_isr_handler(void* arg);
  void process_tag_response(bool poll_mode);
  void process_tag_response_from_interrupt() { process_tag_response(false); }

  // Protocol Helpers
  EspResult<> wait_ready(uint16_t timeout_ms);
  EspResult<> write_command(std::span<const uint8_t> cmd);
  EspResult<> read_ack(uint16_t timeout_ms = 15);

  EspResult<> command(std::span<const uint8_t> cmd, uint16_t timeout_ms = 15);

  // Blocks until ready, reads the frame, and rigorously validates the NXP protocol headers.
  EspResult<> read_response(uint8_t expected_cmd, std::span<uint8_t> response,
                            uint16_t timeout_ms = 100);
};

}  // namespace halpp