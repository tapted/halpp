#pragma once

#include <ctime>

#include "espbase/esp_result.hpp"
#include "halpp/core/default_instance.hpp"
#include "halpp/i2c/i2c_device.hpp"

namespace halpp {

class Pcf85063A : public DefaultInstance<Pcf85063A> {
 public:
  static constexpr uint8_t I2C_ADDRESS_DEFAULT = 0x51;

  explicit Pcf85063A(I2CDevice device = I2CDevice{}) : i2c_dev_(std::move(device)) {}
  ~Pcf85063A() = default;

  static EspResult<> init_default(uint8_t i2c_address = I2C_ADDRESS_DEFAULT);

  EspResult<> begin();
  EspResult<> hardware_reset();
  bool is_initialized() const { return !!i2c_dev_; }

  EspResult<> set_time(const struct tm* time);
  EspResult<> set_date(const struct tm* date);
  EspResult<> set_datetime(const struct tm* datetime);
  EspResult<> read_datetime(struct tm* datetime);

  time_t read_unix_seconds();

  // Enable alarm interrupt.
  EspResult<> enable_alarm();
  EspResult<> disable_alarm();

  EspResult<> get_alarm_flag(bool* flag);
  EspResult<> clear_alarm_flag();

  EspResult<> set_alarm(const struct tm* time);
  EspResult<> read_alarm(struct tm* time);

 private:
  I2CDevice i2c_dev_;
};

}  // namespace halpp
