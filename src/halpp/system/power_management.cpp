#include "halpp/system/power_management.hpp"

#include <atomic>
#include <esp_wifi.h>

#include "espbase/esp_timer.hpp"
#include "halpp/display/spi_display.hpp"

namespace halpp {

static constexpr const char TAG[] = "PowerManager";

constexpr uint32_t FADE_IN_TIME_MS = 200;      // 200ms fade in when waking up
constexpr uint32_t FADE_OUT_TIME_MS = 500;     // 500ms fade out when going to sleep
constexpr uint32_t DISPLAY_OFF_TIME_MS = 200;  // 200ms after backlight hits 0 before cutting power
constexpr uint32_t INACTIVITY_THRESHOLD_MS = 20000;
constexpr uint32_t RADIO_SLEEP_THRESHOLD_MS = 60000;
constexpr uint32_t HIBERNATE_THRESHOLD_MS = 300000;

static std::atomic<uint32_t> lock_screen_fade{0};
static std::atomic<uint32_t> lock_screen_off{0};
static std::atomic<uint32_t> lock_wifi_off{0};
static std::atomic<uint32_t> lock_hibernate{0};

static EspTimer sleep_timer;
static EspTimer deep_sleep_timer;
static EspTimer radio_timer;
static EspTimer hibernate_timer;

static uint32_t current_timeout_ms{20000};
static uint8_t saved_backlight{100};

static bool panel_off{false};
static bool radio_sleeping{false};

bool screen_sleeping{false};
void (*disable_touch_on_screen_sleep_callback)() = nullptr;

static void push_locks(PowerLock flags) {
  if (has_flag(flags, PowerLock::PreventScreenFade))
    lock_screen_fade.fetch_add(1, std::memory_order_relaxed);
  if (has_flag(flags, PowerLock::PreventScreenOff))
    lock_screen_off.fetch_add(1, std::memory_order_relaxed);
  if (has_flag(flags, PowerLock::PreventWifiOff))
    lock_wifi_off.fetch_add(1, std::memory_order_relaxed);
  if (has_flag(flags, PowerLock::PreventHibernate))
    lock_hibernate.fetch_add(1, std::memory_order_relaxed);
}

static void pop_locks(PowerLock flags) {
  if (has_flag(flags, PowerLock::PreventScreenFade))
    lock_screen_fade.fetch_sub(1, std::memory_order_relaxed);
  if (has_flag(flags, PowerLock::PreventScreenOff))
    lock_screen_off.fetch_sub(1, std::memory_order_relaxed);
  if (has_flag(flags, PowerLock::PreventWifiOff))
    lock_wifi_off.fetch_sub(1, std::memory_order_relaxed);
  if (has_flag(flags, PowerLock::PreventHibernate))
    lock_hibernate.fetch_sub(1, std::memory_order_relaxed);
}

static bool is_locked(PowerLock flag) {
  switch (flag) {
    case PowerLock::PreventScreenFade:
      return lock_screen_fade.load(std::memory_order_relaxed) > 0;
    case PowerLock::PreventScreenOff:
      return lock_screen_off.load(std::memory_order_relaxed) > 0;
    case PowerLock::PreventWifiOff:
      return lock_wifi_off.load(std::memory_order_relaxed) > 0;
    case PowerLock::PreventHibernate:
      return lock_hibernate.load(std::memory_order_relaxed) > 0;
    default:
      return false;
  }
}

static void on_sleep_timeout() {
  if (is_locked(PowerLock::PreventScreenFade)) {
    sleep_timer.start_once(current_timeout_ms);
    return;
  }

  if (!screen_sleeping) {
    if (disable_touch_on_screen_sleep_callback) disable_touch_on_screen_sleep_callback();

    screen_sleeping = true;
    saved_backlight = SpiDisplay::default_instance().get_backlight();
    SpiDisplay::default_instance().set_backlight(BacklightState::On, 0, FADE_OUT_TIME_MS);

    // Trigger the hardware kill switch timer
    deep_sleep_timer.start_once(200);  // 200ms display off delay[cite: 5]
  }
}

static void on_deep_sleep_timeout() {
  if (is_locked(PowerLock::PreventScreenOff)) {
    return;  // Abort hardware shutdown, leave panel on
  }
  SpiDisplay::default_instance().set_backlight(BacklightState::Off, 0);
}

static void on_radio_timeout() {
  if (is_locked(PowerLock::PreventWifiOff)) {
    radio_timer.start_once(RADIO_SLEEP_THRESHOLD_MS);
    return;
  }

  if (!radio_sleeping) {
    radio_sleeping = true;
    esp_wifi_stop();
  }
}

static void on_hibernate_timeout() {
  if (is_locked(PowerLock::PreventHibernate)) {
    hibernate_timer.start_once(HIBERNATE_THRESHOLD_MS);
    return;
  }

  deep_sleep_for_seconds(0);
}

void init_power_management() {
  // Bind the class methods directly to the timers without allocations
  sleep_timer.create<&on_sleep_timeout>("sleep_tmr");
  deep_sleep_timer.create<&on_deep_sleep_timeout>("hw_off_tmr");
  radio_timer.create<&on_radio_timeout>("radio_tmr");
  hibernate_timer.create<&on_hibernate_timeout>("hib_tmr");

  power_save_reset_timeout();
  radio_timer.start_once(RADIO_SLEEP_THRESHOLD_MS);
  hibernate_timer.start_once(HIBERNATE_THRESHOLD_MS);
}

bool wake_up_screen(int8_t level) {
  // 1. Wake the radio immediately if it was asleep
  if (radio_sleeping) {
    radio_sleeping = false;
    esp_wifi_start();
    ESP_LOGI(TAG, "Wi-Fi radio powered back up.");
  }

  bool screen_was_sleeping = screen_sleeping;
  if (screen_sleeping) {
    screen_sleeping = false;

    // Abort the hardware shutdown if we caught it mid-fade
    deep_sleep_timer.stop();

    if (panel_off) {
      // Display::instance()->panel_on_off(true);
      panel_off = false;

      // Note: Hardware usually needs a tiny delay to stabilize colors here.
      // The 200ms backlight fade below usually masks this perfectly!
    }
    // Phase 2: Fade the backlight back to its previous brightness
    uint8_t target_backlight = (level >= 0) ? level : saved_backlight;
    SpiDisplay::default_instance().set_backlight(BacklightState::On, target_backlight,
                                                 FADE_IN_TIME_MS);
  }

  // 3. Reset the sleep, radio, and hibernate timers
  sleep_timer.start_once(current_timeout_ms);
  radio_timer.start_once(RADIO_SLEEP_THRESHOLD_MS);
  hibernate_timer.start_once(HIBERNATE_THRESHOLD_MS);

  return screen_was_sleeping;
}

void power_save_set_timeout(uint32_t timeout_ms) {
  current_timeout_ms = timeout_ms;
  // Only restart the countdown if the screen is actually awake[cite: 5]
  if (!screen_sleeping) {
    sleep_timer.start_once(current_timeout_ms);
  }
}

void power_save_reset_timeout() {
  power_save_set_timeout(INACTIVITY_THRESHOLD_MS);
}

void fade_screen_now() {
  if (!screen_sleeping) {
    on_sleep_timeout();  // This will fade the backlight to 0 and start the hardware shutdown timer
  }
}

ScopedPowerDisable::ScopedPowerDisable(PowerLock flags) : flags_(flags) {
  push_locks(flags_);
}

ScopedPowerDisable::~ScopedPowerDisable() {
  pop_locks(flags_);
  // If we just unlocked the screen, simulate a touch to prevent instant sleep.

  // Act as though the screen was just touched, resetting all the timers. Otherwise, the system
  // might go back to sleep immediately, interrupting whatever event just happened to cause the
  // last lock to be released. This will turn wifi back on as well, which is maybe not ideal,
  // but it's better than being unpredicable/glitchy.
  if (has_flag(flags_, PowerLock::PreventScreenFade) ||
      has_flag(flags_, PowerLock::PreventScreenOff)) {
    wake_up_screen();
  }
}

}  // namespace halpp