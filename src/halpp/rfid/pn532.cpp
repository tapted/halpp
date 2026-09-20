#include "halpp/rfid/pn532.hpp"

#include <algorithm>
#include <driver/gpio.h>
#include <mutex>

#include "espbase/main_loop.hpp"
#include "halpp/i2c/i2c_master.hpp"

/*
 * =======================================================================================
 * PN532 I2C RESPONSE FRAME ARCHITECTURE
 * =======================================================================================
 * Note: When reading over I2C, the PN532 physically prepends a 0x01 "Ready" status byte
 * to the beginning of the transmission. This shifts the standard NXP frame right by 1.
 *
 * INDEX | 0    | 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8...       |   |   |
 * ------|------|------|-------------|------|------|------|------|------------|---|---|
 * BYTE  | 0x01 | 0x00 | 0x00 | 0xFF | LEN  | LCS  | 0xD5 | CMD  | DATA       |DCS|00 |
 * ------|------|------|-------------|------|------|------|------|------------|---|---|
 * FIELD | I2C  | PRE  | START CODES | LENG | LCHK | TFI  | CODE | PAYLOAD... |CHK|PST|
 *       | RDY  | AMBL |             | TH   | SUM  |      |      |            |   |   |
 *
 * TFI: 0xD5 = PN532 to Host
 * CMD: Command Code (Original Host Command + 1)
 * =======================================================================================
 */

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

EspResult<> Pn532::init_default(uint8_t i2c_address, gpio_num_t irq_pin, gpio_num_t rstpdn_pin) {
  std::lock_guard<std::mutex> lock(default_mutex());
  if (default_optional()) return ESP_OK;
  Pn532& inst = emplace_default_instance(lock, I2CDevice{}, irq_pin, rstpdn_pin);

  if (EspError err = EspError::check(
          I2CMaster::instance().add_device(i2c_address, PN532_CLOCK_SPEED), &inst.i2c_dev_)) {
    default_optional().reset();
    return err.log(TAG, "Failed to add default PN532 to I2C bus");
  }

  return inst.begin();
}

EspResult<> Pn532::hardware_reset() {
  if (rstpdn_pin_ == GPIO_NUM_NC) return ESP_ERR_NOT_SUPPORTED;

  // Configure pin if not done already
  gpio_reset_pin(rstpdn_pin_);
  gpio_set_direction(rstpdn_pin_, GPIO_MODE_OUTPUT);

  gpio_hold_dis(rstpdn_pin_);  // Ensure it's not held from a prior light-sleep prevention.

  gpio_set_level(rstpdn_pin_, 0);
  vTaskDelay(pdMS_TO_TICKS(10));

  // Pull HIGH to boot up
  gpio_set_level(rstpdn_pin_, 1);
  vTaskDelay(pdMS_TO_TICKS(10));

  gpio_hold_en(rstpdn_pin_);  // Keep the device alive during light sleep.

  // Re-initialize the chip state
  return wake_up();
}

EspResult<> Pn532::begin() {
  if (!i2c_dev_) return ESP_ERR_INVALID_STATE;

  if (irq_pin_ != GPIO_NUM_NC) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << irq_pin_),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE  // Fire when the line drops LOW
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(irq_pin_, gpio_isr_handler, this);
  }

  if (rstpdn_pin_ != GPIO_NUM_NC) {
    ESP_LOGI(TAG, "Hardware Reset pin configured. Power-cycling PN532...");
    return hardware_reset();
  }

  ESP_LOGW(TAG, "No Reset pin configured. Relying on software wake-up.");
  return wake_up();
}

void IRAM_ATTR Pn532::gpio_isr_handler(void* arg) {
  Pn532* inst = static_cast<Pn532*>(arg);
  if (!inst->task_pending_) {
    inst->task_pending_ = true;
    main_loop.push<&Pn532::process_tag_response_from_interrupt>(inst);
  }
}

void Pn532::start_passive_target_read() {
  if (irq_pin_ == GPIO_NUM_NC) {
    ESP_LOGW(TAG, "IRQ pin not configured. Cannot start passive target read.");
    return;
  }

  // 1. Ensure ISR is off so the ACK doesn't trigger a false positive
  gpio_intr_disable(irq_pin_);
  scanning_ = false;

  // 2. Write command
  constexpr uint8_t cmd[] = {0x4A, 0x01, 0x00};
  if (EspError err = command(cmd)) {
    err.log(TAG, "start_passive_target_read command");
    return;
  }
  // 4. Command was accepted. Arm the ISR for the actual tag response!
  scanning_ = true;
  gpio_intr_enable(irq_pin_);
}

EspResult<> Pn532::process_tag_response(bool poll_mode) {
  if (!scanning_) return ESP_ERR_INVALID_STATE;

  // A 10ms timeout is plenty for the ESP32 hardware driver to attempt the read.
  auto status = i2c_dev_.read_byte(10);

  if (!status) {  // I2C hardware error (timeout, bus locked, etc.)
    consecutive_fails_++;

    // If we fail 10 times in a row (~500ms of polling), the bus is dead.
    if (consecutive_fails_ >= 10 && rstpdn_pin_ != GPIO_NUM_NC) {
      ESP_LOGE(TAG, "I2C Deadlock detected! Executing hardware reset via RSTPDN pin...");

      hardware_reset();
      start_passive_target_read();  // Restart the background scan.

      consecutive_fails_ = 0;  // Reset the counter
    } else if (!poll_mode) {
      ESP_LOGW(TAG, "I2C read failed (%s). Consecutive errors: %d", esp_err_to_name(status.error()),
               consecutive_fails_);
    }
    return status.strip().log_error(TAG, "process_tag_response status");
  }

  // A successful I2C read happened (even if it's just a 0x00 "Not Ready" byte)
  consecutive_fails_ = 0;
  if (*status == 0x00) return ESP_OK;  // Not ready.
  if (*status != 0x01) {
    ESP_LOGW(TAG, "Unexpected status byte: %02X", *status);
    return ESP_ERR_INVALID_STATE;
  }

  // Data is confirmed ready. Now we can safely mark the scan as complete.
  scanning_ = false;

  // Read full response frame safely in task context
  uint8_t response[32] = {0};
  if (EspError err = i2c_dev_.rx(response)) return err.log(TAG, "process_tag_response");

  // Verify protocol command and tag count
  if (response[7] != 0x4B || response[8] == 0)
    return EspResult<>(ESP_ERR_INVALID_RESPONSE).log_error(TAG, "invalid response");

  // Extract hardware identifiers
  uint16_t atqa = (response[10] << 8) | response[11];
  uint8_t sak = response[12];
  uint8_t uid_len = response[13];

  const char* type = "Unknown ISO14443A tag";
  if (sak == 0x08 && uid_len == 4) {
    type = "MIFARE Classic 1K (Fob/Card)";
  } else if (sak == 0x00 && uid_len == 7) {
    type = "NTAG / MIFARE Ultralight (Sticker/Tag)";
  } else if (sak == 0x20 && atqa == 0x0344) {
    type = "MIFARE DESFire (Opal card?)";
  } else if (sak == 0x20 && atqa == 0x0048) {
    type = "EMV Contactless Payment (Card/Device)";
  } else if (sak == 0x20 && atqa == 0x0004) {
    type = "Phone?";
  }
  ESP_LOGI(TAG, "Tag -> ATQA: %04X, SAK: %02X, UID Len: %d, Type: %s", atqa, sak, uid_len, type);

  if (on_tag_cb_ && uid_len <= 7) {
    on_tag_cb_(on_tag_ctx_, *this, std::span<const uint8_t>(&response[14], uid_len));
  }
  return ESP_OK;
}

EspResult<> Pn532::set_infinite_retries() {
  // RFConfiguration (0x32) -> Config Item 5 (MaxRetries)
  // Byte 3: MxRtyATR (0xFF = default)
  // Byte 4: MxRtyPSL (0x01 = default)
  // Byte 5: MxRtyPassiveActivation (0xFF = INFINITE)
  constexpr uint8_t cmd[] = {0x32, 0x05, 0xFF, 0x01, 0xFF};
  if (EspError err = command(cmd, 15)) return err.log(TAG, "set_infinite_retries command");
  uint8_t response[14] = {0};
  return read_response(0x33, response, 100).log_error(TAG, "set_infinite_retries response");
}

EspResult<> Pn532::sam_config() {
  // SAMConfiguration: Normal Mode, enable IRQ.
  return command({0x14, 0x01, 0x14, 0x01}).log_error(TAG, "sam_config");
}

EspResult<> Pn532::power_down() {
  // Acknowledge the command. After this ACK, the PN532 halts the RF field and sleeps.
  return command({0x16, 0x00}).log_error(TAG, "power_down");
}

EspResult<> Pn532::wake_up() {
  // Dummy ping to wake the oscillator (will intentionally NACK if asleep)
  (void)i2c_dev_.read_byte(10);
  vTaskDelay(pdMS_TO_TICKS(5));

  // Drain stuck TX buffer from previous crashes
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

  // Bus is now completely synchronized. Send the configuration.
  if (EspError err = sam_config()) return err.log(TAG, "wake_up sam_config");
  if (EspError err = set_infinite_retries()) return err.log(TAG, "wake_up set_infinite_retries");
  ESP_LOGI(TAG, "wake_up complete: infinite retries set");
  return ESP_OK;
}

EspResult<> Pn532::get_firmware_version(std::array<uint8_t, 4>& version_out) {
  if (EspError err = command({0x02})) return err.log(TAG, "firmware command");
  uint8_t response[14] = {0};
  if (EspError err = read_response(0x03, response, 100)) return err.log(TAG, "firmware response");

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

  return i2c_dev_.tx(std::span(buffer, 8 + cmd.size())).log_error(TAG, "write_command");
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

EspResult<> Pn532::command(std::span<const uint8_t> cmd, uint16_t timeout_ms) {
  if (EspError err = write_command(cmd)) return err;
  return read_ack(timeout_ms);
}

EspResult<> Pn532::read_response(uint8_t expected_cmd, std::span<uint8_t> response,
                                 uint16_t timeout_ms) {
  if (EspError err = wait_ready(timeout_ms)) return err;
  if (EspError err = i2c_dev_.rx(response, timeout_ms)) return err;

  // 1. Validate the rigid NXP frame structure (Preamble, Start Codes, and TFI)
  if (response[1] != 0x00 || response[2] != 0x00 || response[3] != 0xFF || response[6] != 0xD5) {
    ESP_LOGW(TAG, "Malformed PN532 frame. PRE: %02X, ST1: %02X, ST2: %02X, TFI: %02X", response[1],
             response[2], response[3], response[6]);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // 2. Validate the specific command response
  if (response[7] != expected_cmd) {
    ESP_LOGW(TAG, "Command mismatch. Expected 0x%02X, Got 0x%02X", expected_cmd, response[7]);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // (Optional) You could also validate the Length (response[4]) and Checksums here!

  return ESP_OK;
}

}  // namespace halpp

// clang-format off
/**
 * NXP PN532 Command Reference
 * | Command (Hex) | Name | Response (Hex) | Description |
 * | --- | --- | --- | --- |
 * | **`0x00`** | `Diagnose` | **`0x01`** | Run self-tests and communication line diagnostics. |
 * | **`0x02`** | `GetFirmwareVersion` | **`0x03`** | Returns IC identifier, version, and protocol support mask. |
 * | **`0x04`** | `GetGeneralStatus` | **`0x05`** | Returns current SAM state, field status, and tag memory limits. |
 * | **`0x06`** | `ReadRegister` | **`0x07`** | Read internal 8051 CPU registers or GPIO states. |
 * | **`0x08`** | `WriteRegister` | **`0x09`** | Write to internal 8051 CPU registers or GPIO pins. |
 * | **`0x0C`** | `ReadGPIO` | **`0x0D`** | Read hardware states of the P3 / P7 auxiliary pins. |
 * | **`0x0E`** | `WriteGPIO` | **`0x0F`** | Set hardware states of the P3 / P7 auxiliary pins. |
 * | **`0x10`** | `SetSerialBaudRate` | **`0x11`** | Change UART baud rate (Ignored over I2C). |
 * | **`0x12`** | `SetParameters` | **`0x13`** | Toggle automatic RF behaviors (e.g., auto-removing parity bits). |
 * | **`0x14`** | `SAMConfiguration` | **`0x15`** | Configure Secure Access Module, enable RF field, and route IRQ pin. |
 * | **`0x16`** | `PowerDown` | **`0x17`** | Put chip into deep sleep (Requires `WUP` packet to wake). |
 * | **`0x32`** | `InJumpForDEP` | **`0x33`** | Initiator: Configure NFC Peer-to-Peer mode setup. |
 * | **`0x40`** | `InDataExchange` | **`0x41`** | Initiator: Read/Write data blocks on a currently selected tag. |
 * | **`0x42`** | `InCommunicateThru` | **`0x43`** | Initiator: Send raw low-level RF transmission (bypassing protocol). |
 * | **`0x44`** | `InDeselect` | **`0x45`** | Initiator: Put the currently communicating tag back to sleep. |
 * | **`0x46`** | `InRelease` | **`0x47`** | Initiator: Completely release the current tag from memory. |
 * | **`0x48`** | `InSelect` | **`0x49`** | Initiator: Wake up a specific sleeping tag in the RF field. |
 * | **`0x4A`** | `InListPassiveTarget` | **`0x4B`** | Initiator: Poll the RF field for new tags and retrieve their UIDs. |
 * | **`0x50`** | `InAutoPoll` | **`0x51`** | Initiator: Continuously loop polling for multiple tag frequencies. |
 * | **`0x8C`** | `TgInitAsTarget` | **`0x8D`** | Target: Emulate a physical tag (Card Emulation mode). |
 * | **`0x8E`** | `TgSetGeneralBytes` | **`0x8F`** | Target: Set payload data for NFC Peer-to-Peer targets. |
 * | **`0x90`** | `TgGetData` | **`0x91`** | Target: Receive an incoming payload from an Initiator reader. |
 * | **`0x92`** | `TgSetData` | **`0x93`** | Target: Send a payload out to the Initiator reader. |
 * | **`0x94`** | `TgSetMetaData` | **`0x95`** | Target: Configure Card Emulation parameters. |
 * | **`0x96`** | `TgGetInitiatorCommand` | **`0x97`** | Target: Read raw incoming RF command. |
 * | **`0x98`** | `TgResponseToInitiator` | **`0x99`** | Target: Send raw outgoing RF response. |
 * | **`0x9A`** | `TgGetTargetStatus` | **`0x9B`** | Target: Check active connection state to the Initiator. |
 */