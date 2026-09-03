#pragma once

#include <cstdint>

// Must be provided by embedder.
void deep_sleep_for_seconds(uint32_t seconds);

namespace halpp {

extern bool screen_sleeping;
extern void (*disable_touch_on_screen_sleep_callback)();

void init_power_management();

bool wake_up_screen(int8_t level = -1);  // Returns true if we woke the screen up from sleep mode
void power_save_set_timeout(uint32_t timeout_ms);
void power_save_reset_timeout();
void fade_screen_now();

enum class PowerLock : uint8_t {
  None = 0,
  PreventScreenFade = 1 << 0,
  PreventScreenOff = 1 << 1,
  PreventWifiOff = 1 << 2,
  PreventHibernate = 1 << 3,
  PreventAll = 0x0F
};

constexpr PowerLock operator|(PowerLock a, PowerLock b) {
  return static_cast<PowerLock>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr PowerLock operator&(PowerLock a, PowerLock b) {
  return static_cast<PowerLock>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
constexpr bool has_flag(PowerLock mask, PowerLock flag) {
  return (mask & flag) != PowerLock::None;
}

class ScopedPowerDisable {
 public:
  explicit ScopedPowerDisable(PowerLock flags = PowerLock::PreventAll);

  ~ScopedPowerDisable();

  ScopedPowerDisable(const ScopedPowerDisable&) = delete;
  ScopedPowerDisable& operator=(const ScopedPowerDisable&) = delete;

 private:
  PowerLock flags_;
};

}  // namespace halpp