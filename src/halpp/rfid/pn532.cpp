#include "halpp/rfid/pn532.hpp"

#include <algorithm>
#include <driver/gpio.h>
#include <mutex>

#include "espbase/main_loop.hpp"
#include "halpp/i2c/i2c_master.hpp"

namespace halpp {

static constexpr const char TAG[] = "halpp::PN532";
static constexpr uint8_t PN532_PREAMBLE = 0x00;
static constexpr uint8_t PN532_STARTCODE1 = 0x00;
static constexpr uint8_t PN532_STARTCODE2 = 0xFF;
static constexpr uint8_t PN532_POSTAMBLE = 0x00;
static constexpr uint8_t PN532_HOSTTOPN532 = 0xD4;
static constexpr uint8_t PN532_PN532TOHOST = 0xD5;
static constexpr uint32_t PN532_CLOCK_SPEED = 100000;
static constexpr std::array<uint8_t, 6> PN532_ACK = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

Pn532::~Pn532() {
  if (irq_pin_ != GPIO_NUM_NC) gpio_isr_handler_remove(irq_pin_);
}

EspResult<> Pn532::init_default(uint8_t i2c_address, gpio_num_t irq_pin) {
  std::lock_guard<std::mutex> lock(default_mutex());
  if (default_optional()) return ESP_OK;
  Pn532& inst = emplace_default_instance(lock, I2CDevice{}, irq_pin);

  if (EspError err = EspError::check(
          I2CMaster::instance().add_device(i2c_address, PN532_CLOCK_SPEED), &inst.i2c_dev_)) {
    default_optional().reset();
    return err.log(TAG, "Failed to add default PN532 to I2C bus");
  }

  return inst.begin();
}

EspResult<> Pn532::begin() {
  if (!i2c_dev_) return ESP_ERR_INVALID_STATE;

  if (irq_pin_ != GPIO_NUM_NC) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << irq_pin_),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Keep disabled until we actually scan
    };
    gpio_config(&io_conf);

    // Assumes the global ISR service is started elsewhere in espbase/halpp
    gpio_isr_handler_add(irq_pin_, gpio_isr_handler, this);
  }

  return wake_up();
}

void IRAM_ATTR Pn532::gpio_isr_handler(void* arg) {
  Pn532* inst = static_cast<Pn532*>(arg);

  // Disable the interrupt immediately so it doesn't bounce or double-fire
  gpio_intr_disable(inst->irq_pin_);

  // Post the processing task to the main loop context.
  main_loop.push<&Pn532::process_tag_response_from_interrupt>(inst);
}

EspResult<> Pn532::start_passive_target_read() {
  if (irq_pin_ == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;

  // 1. Ensure ISR is off so the ACK doesn't trigger a false positive
  gpio_intr_disable(irq_pin_);
  scanning_ = false;

  // 2. Write command
  uint8_t cmd[] = {0x4A, 0x01, 0x00};
  if (EspError err = write_command(cmd)) return err.log(TAG, "start_passive_target_read command");

  // 3. Read ACK synchronously (takes < 2ms)
  if (EspError err = read_ack(15)) return err.log(TAG, "start_passive_target_read read_ack");

  // 4. Command was accepted. Arm the ISR for the actual tag response!
  scanning_ = true;
  gpio_intr_enable(irq_pin_);

  return ESP_OK;
}

void Pn532::process_tag_response(bool poll_mode) {
  if (!scanning_) return;

  // A 10ms timeout is plenty for the ESP32 hardware driver to attempt the read.
  auto status = i2c_dev_.read_byte(10);

  if (!status || *status != 0x01) {
    if (!poll_mode) {
      // Only complain if we expected data because the physical IRQ pin dropped
      ESP_LOGW(TAG, "ISR fired but PN532 I2C ready byte not set");
    }
    return;  // Data not ready. Keep scanning_ = true so we can check again.
  }

  // 2. Data is confirmed ready. Now we can safely mark the scan as complete.
  scanning_ = false;

  // 2. Read full response frame safely in task context
  uint8_t response[32] = {0};
  if (EspError err = i2c_dev_.rx(response)) {
    err.log(TAG, "Failed to read PN532 response frame");
    return;
  }

  ESP_LOGI(TAG, "Tag response -> Len: %02X, TFI: %02X, CMD: %02X, Tags: %02X", response[4],
           response[6], response[7], response[8]);

  // 3. Verify protocol command and tag count
  if (response[7] != 0x4B || response[8] == 0) return;

  // 4. Extract UID and fire application callback
  uint8_t uid_len = response[13];
  if (on_tag_cb_ && uid_len <= 7) {
    on_tag_cb_(on_tag_ctx_, *this, std::span<const uint8_t>(&response[14], uid_len));
  }
}

EspResult<> Pn532::sam_config() {
  uint8_t cmd[] = {0x14, 0x01, 0x14, 0x01};  // SAMConfiguration: Normal Mode

  if (EspError err = write_command(cmd)) return err.log(TAG, "sam_config write_command");
  if (EspError err = read_ack()) return err.log(TAG, "sam_config read_ack");

  return ESP_OK;
}

EspResult<> Pn532::read_ack(uint16_t timeout_ms) {
  if (EspError err = wait_ready(timeout_ms)) return err;

  uint8_t buffer[7] = {0};
  if (EspError err = i2c_dev_.rx(buffer)) return err;

  for (int i = 0; i < 6; i++) {
    if (buffer[i + 1] != PN532_ACK[i]) return ESP_ERR_INVALID_RESPONSE;
  }
  return ESP_OK;
}

EspResult<> Pn532::power_down() {
  uint8_t cmd[] = {0x16, 0x00};  // Command: PowerDown, Param: Wake on any interface
  if (EspError err = write_command(cmd)) return err;

  // Acknowledge the command. After this ACK, the PN532 halts the RF field and sleeps.
  return read_ack();
}

EspResult<> Pn532::wake_up() {
  // 1. Dummy ping to wake the oscillator (will intentionally NACK if asleep)
  (void)i2c_dev_.read_byte(10);
  vTaskDelay(pdMS_TO_TICKS(5));

  // 2. Drain stuck TX buffer from previous crashes
  // The PN532 returns 0x00 when its buffer is totally empty.
  // Read until we see 4 consecutive 0x00s to ensure we didn't just hit a 0x00 inside a valid frame.
  int consecutive_zeros = 0;
  for (int i = 0; i < 64; i++) {
    auto val = i2c_dev_.read_byte(10);
    if (!val) break;  // A NACK means the bus is safely idle

    if (*val == 0x00) {
      consecutive_zeros++;
      if (consecutive_zeros >= 4) break;
    } else {
      consecutive_zeros = 0;
    }
  }

  // 3. Bus is now completely synchronized. Send the configuration.
  return sam_config();
}

EspResult<> Pn532::get_firmware_version(std::array<uint8_t, 4>& version_out) {
  uint8_t cmd[] = {0x02};  // Command: GetFirmwareVersion

  if (EspError err = write_command(cmd)) return err.log(TAG, "firmware write_command");
  if (EspError err = read_ack()) return err.log(TAG, "firmware read_ack");
  if (EspError err = wait_ready(100)) return err.log(TAG, "firmware wait_ready");

  // Frame: 1 Status, 1 Preamble, 2 Start, 1 Len, 1 LCS, 1 TFI, 1 CMD, 4 Data, 1 DCS, 1 Post
  uint8_t response[14] = {0};
  if (EspError err = i2c_dev_.rx(response, 100)) return err;

  // response[6] is TFI (0xD5). response[7] is the Command Code (0x03).
  if (response[7] != 0x03) {
    ESP_LOGE(TAG, "expected 0x03, got 0x%02X", response[7]);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Byte 8: IC version (Should be 0x32 for PN532)
  // Byte 9: Firmware Version
  // Byte 10: Firmware Revision
  // Byte 11: Supported features
  std::copy_n(&response[8], 4, version_out.begin());

  return ESP_OK;
}

EspResult<> Pn532::wait_ready(uint16_t timeout_ms) {
  uint16_t timer = 0;

  while (timer < timeout_ms) {
    auto status = i2c_dev_.read_byte();
    if (status && *status == 0x01) {  // 0x01 indicates PN532 is ready to transmit data
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
    timer += 5;
  }
  return ESP_ERR_TIMEOUT;
}

EspResult<> Pn532::write_command(std::span<const uint8_t> cmd) {
  if (cmd.size() > 56) return ESP_ERR_INVALID_ARG;

  uint8_t buffer[64] = {0};

  buffer[0] = PN532_PREAMBLE;
  buffer[1] = PN532_STARTCODE1;
  buffer[2] = PN532_STARTCODE2;

  uint8_t length = cmd.size() + 1;  // +1 for TFI
  buffer[3] = length;
  buffer[4] = ~length + 1;  // Length checksum

  buffer[5] = PN532_HOSTTOPN532;
  uint8_t sum = PN532_HOSTTOPN532;

  for (size_t i = 0; i < cmd.size(); i++) {
    buffer[6 + i] = cmd[i];
    sum += cmd[i];
  }

  buffer[6 + cmd.size()] = ~sum + 1;  // Data checksum
  buffer[7 + cmd.size()] = PN532_POSTAMBLE;

  return i2c_dev_.transmit(buffer, 8 + cmd.size()).log_error(TAG, "write_command");
}

}  // namespace halpp