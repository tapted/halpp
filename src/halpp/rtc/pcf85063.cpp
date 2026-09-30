#include "halpp/rtc/pcf85063.hpp"

#include <esp_log.h>

#include "halpp/i2c/i2c_master.hpp"

static constexpr char TAG[] = "PCF85063";

namespace halpp {
namespace {

// PCF85063 I2C address and registers
static constexpr uint32_t PCF85063_I2C_SPEED_HZ = 400000;

// Register addresses
static constexpr uint8_t RTC_CTRL_1_ADDR = 0x00;
static constexpr uint8_t RTC_CTRL_2_ADDR = 0x01;
static constexpr uint8_t RTC_OFFSET_ADDR = 0x02;
static constexpr uint8_t RTC_RAM_ADDR = 0x03;
static constexpr uint8_t RTC_SECOND_ADDR = 0x04;
static constexpr uint8_t RTC_MINUTE_ADDR = 0x05;
static constexpr uint8_t RTC_HOUR_ADDR = 0x06;
static constexpr uint8_t RTC_DAY_ADDR = 0x07;
static constexpr uint8_t RTC_WDAY_ADDR = 0x08;
static constexpr uint8_t RTC_MONTH_ADDR = 0x09;
static constexpr uint8_t RTC_YEAR_ADDR = 0x0A;
static constexpr uint8_t RTC_SECOND_ALARM = 0x0B;
static constexpr uint8_t RTC_MINUTE_ALARM = 0x0C;
static constexpr uint8_t RTC_HOUR_ALARM = 0x0D;
static constexpr uint8_t RTC_DAY_ALARM = 0x0E;
static constexpr uint8_t RTC_WDAY_ALARM = 0x0F;
static constexpr uint8_t RTC_TIMER_VAL = 0x10;
static constexpr uint8_t RTC_TIMER_MODE = 0x11;

// Control register 1 bits
static constexpr uint8_t RTC_CTRL_1_STOP = 0x20;
static constexpr uint8_t RTC_CTRL_1_SR = 0x10;
static constexpr uint8_t RTC_CTRL_1_CAP_SEL = 0x01;  // 0=7pF, 1=12.5pF

// Control register 2 bits
static constexpr uint8_t RTC_CTRL_2_AIE = 0x80;  // Alarm interrupt enable
static constexpr uint8_t RTC_CTRL_2_AF = 0x40;   // Alarm flag

static constexpr uint8_t RTC_ALARM = 0x80;
static constexpr uint16_t YEAR_OFFSET = 1970;

// BCD conversion helpers
constexpr uint8_t dec_to_bcd(int val) {
  return static_cast<uint8_t>((val / 10 * 16) + (val % 10));
}

constexpr int bcd_to_dec(uint8_t val) {
  return int{(val / 16 * 10) + (val % 16)};
}

}  // namespace

EspResult<> Pcf85063A::init_default(uint8_t i2c_addr) {
  std::lock_guard<std::mutex> lock(default_mutex());
  if (default_optional()) return ESP_OK;

  EspResult<I2CDevice> device = I2CMaster::instance().add_device(i2c_addr, PCF85063_I2C_SPEED_HZ);
  if (!device) return device.strip().log_error(TAG, "Failed to add PCF85063 device");

  Pcf85063A& inst = emplace_default_instance(lock, std::move(*device));
  return inst.begin();
}

EspResult<> Pcf85063A::begin() {
  // Configure RTC: Normal mode, RTC run, no reset, 24hr format, 12.5pF capacitance
  auto result = i2c_dev_.write_reg(RTC_CTRL_1_ADDR, RTC_CTRL_1_CAP_SEL);
  if (!result) return result.log_error(TAG, "Failed write: RTC_CTRL_1_CAP_SEL");

  ESP_LOGI(TAG, "RTC initialized");
  return ESP_OK;
}

EspResult<> Pcf85063A::hardware_reset() {
  return i2c_dev_.write_reg(RTC_CTRL_1_ADDR, RTC_CTRL_1_CAP_SEL | RTC_CTRL_1_SR)
      .log_error(TAG, "Failed write: reset");
}

EspResult<> Pcf85063A::set_time(const struct tm* time) {
  if (time == nullptr) return ESP_ERR_INVALID_ARG;

  uint8_t buf[3] = {
      dec_to_bcd(time->tm_sec),
      dec_to_bcd(time->tm_min),
      dec_to_bcd(time->tm_hour),
  };
  return i2c_dev_.write_reg(RTC_SECOND_ADDR, buf).log_error(TAG, "Failed write: set_time");
}

static void fix_week_day(struct tm* date) {
  if (date != nullptr) {
    time_t tmp = mktime(const_cast<struct tm*>(date));
    struct tm tmp_tm;
    localtime_r(&tmp, &tmp_tm);
    date->tm_wday = tmp_tm.tm_wday;
  }
}

EspResult<> Pcf85063A::set_date(const struct tm* date_in) {
  if (date_in == nullptr) return ESP_ERR_INVALID_ARG;

  struct tm date = *date_in;
  // fix week day, this data could be incorrect
  fix_week_day(&date);

  // tm_mon is 0-11, RTC expects 1-12
  // tm_year is years since 1900, RTC expects years since 1970
  uint8_t buf[4] = {
      dec_to_bcd(date.tm_mday),
      dec_to_bcd(date.tm_wday),
      dec_to_bcd(date.tm_mon + 1),
      dec_to_bcd((date.tm_year + 1900) - YEAR_OFFSET),
  };
  return i2c_dev_.write_reg(RTC_DAY_ADDR, buf).log_error(TAG, "Failed write: RTC_DAY_ADDR");
}

EspResult<> Pcf85063A::set_datetime(const struct tm* datetime_in) {
  if (datetime_in == nullptr) return ESP_ERR_INVALID_ARG;

  // fix week day, this data could be incorrect
  struct tm datetime = *datetime_in;
  fix_week_day(&datetime);

  // tm_mon is 0-11, RTC expects 1-12
  // tm_year is years since 1900, RTC expects years since 1970
  uint8_t buf[7] = {
      dec_to_bcd(datetime.tm_sec),
      dec_to_bcd(datetime.tm_min),
      dec_to_bcd(datetime.tm_hour),
      dec_to_bcd(datetime.tm_mday),
      dec_to_bcd(datetime.tm_wday),
      dec_to_bcd(datetime.tm_mon + 1),
      dec_to_bcd((datetime.tm_year + 1900) - YEAR_OFFSET),
  };
  return i2c_dev_.write_reg(RTC_SECOND_ADDR, buf).log_error(TAG, "Failed write: RTC_SECOND_ADDR");
}

EspResult<> Pcf85063A::read_datetime(struct tm* datetime) {
  if (datetime == nullptr) return ESP_ERR_INVALID_ARG;

  uint8_t buf[7] = {0};
  auto result = i2c_dev_.read_reg(RTC_SECOND_ADDR, buf);
  if (!result) return result.log_error(TAG, "Failed read: RTC_SECOND_ADDR");

  *datetime = tm{};  // Clear the struct first

  // Fill in the fields
  datetime->tm_sec = bcd_to_dec(buf[0] & 0x7F);
  datetime->tm_min = bcd_to_dec(buf[1] & 0x7F);
  datetime->tm_hour = bcd_to_dec(buf[2] & 0x3F);
  datetime->tm_mday = bcd_to_dec(buf[3] & 0x3F);
  datetime->tm_wday = bcd_to_dec(buf[4] & 0x07);
  datetime->tm_mon = bcd_to_dec(buf[5] & 0x1F) - 1;  // RTC uses 1-12, tm_mon is 0-11
  int year = bcd_to_dec(buf[6]) + YEAR_OFFSET;
  datetime->tm_year = year - 1900;  // tm_year is years since 1900
  datetime->tm_isdst = -1;          // Unknown DST status

  return ESP_OK;
}

time_t Pcf85063A::read_unix_seconds() {
  struct tm datetime;
  if (EspError err = read_datetime(&datetime)) {
    err.log(TAG, "Failed to read datetime");
    return -1;
  }
  return mktime(&datetime);
}

EspResult<> Pcf85063A::enable_alarm() {
  return i2c_dev_.write_reg(RTC_CTRL_2_ADDR, RTC_CTRL_2_AIE)
      .log_error(TAG, "Failed write: enable_alarm");
}

EspResult<> Pcf85063A::disable_alarm() {
  return i2c_dev_.write_reg(RTC_CTRL_2_ADDR, 0).log_error(TAG, "Failed write: disable_alarm");
}

EspResult<> Pcf85063A::get_alarm_flag(bool* flag) {
  if (flag == nullptr) return ESP_ERR_INVALID_ARG;

  auto result = i2c_dev_.read_reg(RTC_CTRL_2_ADDR);
  if (!result) return result.strip().log_error(TAG, "Failed read: RTC_CTRL_2");

  *flag = (*result & RTC_CTRL_2_AF) != 0;
  return ESP_OK;
}

EspResult<> Pcf85063A::clear_alarm_flag() {
  auto result = i2c_dev_.read_reg(RTC_CTRL_2_ADDR);
  if (!result) return result.strip().log_error(TAG, "Failed read: RTC_CTRL_2");
  return i2c_dev_.write_reg(RTC_CTRL_2_ADDR, *result & ~RTC_CTRL_2_AF)
      .log_error(TAG, "Failed write: clear_alarm_flag");
}

EspResult<> Pcf85063A::set_alarm(const struct tm* time) {
  if (time == nullptr) return ESP_ERR_INVALID_ARG;

  uint8_t buf[5] = {
      static_cast<uint8_t>(dec_to_bcd(time->tm_sec) & (~RTC_ALARM)),
      static_cast<uint8_t>(dec_to_bcd(time->tm_min) & (~RTC_ALARM)),
      static_cast<uint8_t>(dec_to_bcd(time->tm_hour) & (~RTC_ALARM)),
      RTC_ALARM,  // Disable day alarm
      RTC_ALARM   // Disable weekday alarm
  };
  return i2c_dev_.write_reg(RTC_SECOND_ALARM, buf).log_error(TAG, "Failed write: set_alarm");
}

EspResult<> Pcf85063A::read_alarm(struct tm* time) {
  if (time == nullptr) return ESP_ERR_INVALID_ARG;

  uint8_t buf[5] = {0};
  auto result = i2c_dev_.read_reg(RTC_SECOND_ALARM, buf);
  if (!result) return result.log_error(TAG, "Failed read: RTC_SECOND_ALARM");

  *time = tm{};  // Clear the struct first

  time->tm_sec = bcd_to_dec(buf[0] & 0x7F);
  time->tm_min = bcd_to_dec(buf[1] & 0x7F);
  time->tm_hour = bcd_to_dec(buf[2] & 0x3F);
  time->tm_mday = bcd_to_dec(buf[3] & 0x3F);
  time->tm_wday = bcd_to_dec(buf[4] & 0x07);
  time->tm_isdst = -1;

  return ESP_OK;
}

}  // namespace halpp