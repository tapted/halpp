#include "halpp/led_strip/led_effects.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <esp_timer.h>
#include <variant>

#include "espbase/main_loop_task.hpp"
#include "halpp/led_strip/led_strip.hpp"

namespace halpp {
namespace {

struct RainbowData {
  uint32_t cycle_ms = 18000;  // 18 seconds for a full 360-degree sweep
};

struct FlashData {
  int toggles_left;
  bool state;
  uint8_t r, g, b;
};

struct BreatheData {
  uint32_t tick = 0;
  uint16_t hue = 0;
};

struct LightAnimData {
  LedStrip* strip;
  // std::monostate represents "no animation active"
  std::variant<std::monostate, RainbowData, FlashData, BreatheData> current_effect;
};

static constexpr auto gamma8 = []() {
  std::array<uint8_t, 256> table{};
  for (int i = 0; i < 256; ++i) {
    float normalized = i / 255.0f;
    // Gamma correction with gamma=3.0 (2.8 is standard, but needs std::pow).
    float gamma_corrected = normalized * normalized * normalized;
    table[i] = static_cast<uint8_t>((gamma_corrected * 255.0f) + 0.5f);
  }
  return table;
}();

static std::optional<MainLoopTask<LightAnimData>> anim_task;
static LightAnimData anim_data;

static std::optional<uint32_t> rainbow_step(MainLoopTask<LightAnimData>& task) {
  auto& rb = std::get<RainbowData>(task.data()->current_effect);
  int64_t now_ms = esp_timer_get_time() / 1000;
  uint16_t hue = (now_ms % rb.cycle_ms) * 360 / rb.cycle_ms;  // Interpolate.
  task.data()->strip->set_pixel_hsv(0, hue, 200, 200);
  return 50;
}

static std::optional<uint32_t> flash_step(MainLoopTask<LightAnimData>& task) {
  auto& fd = std::get<FlashData>(task.data()->current_effect);

  if (fd.toggles_left <= 0) {
    // Restore the final ON state before terminating
    task.data()->strip->set_pixel(0, fd.r, fd.g, fd.b);
    task.data()->strip->refresh();
    return std::nullopt;  // End the task
  }

  fd.state = !fd.state;
  if (fd.state) {
    task.data()->strip->set_pixel(0, fd.r, fd.g, fd.b);
  } else {
    task.data()->strip->set_pixel(0, 0, 0, 0);
  }

  task.data()->strip->refresh();
  fd.toggles_left--;

  return 250;  // Toggle every 250ms
}

static std::optional<uint32_t> breathe_step(MainLoopTask<LightAnimData>& task) {
  auto& bd = std::get<BreatheData>(task.data()->current_effect);

  // Create a 512-step cycle: 0 -> 255 -> 0
  uint16_t phase = bd.tick % 512;
  uint8_t linear_brightness = (phase < 256) ? phase : 511 - phase;

  // Map the linear 0-255 wave through the logarithmic curve
  uint8_t corrected_brightness = gamma8[linear_brightness];

  // Assumes a single LED at index 0
  task.data()->strip->set_pixel_hsv(0, bd.hue, 255, corrected_brightness);
  task.data()->strip->refresh();

  // Incrementing by larger numbers speeds up the breathing cycle
  bd.tick += 1;

  return 25;
}

// ### Single-Pixel LED Effects
//
// | **Candle** | Fix the `hue` to a yellow/orange value, and assign random values to `val` between
// a defined minimum and maximum using `esp_random()`. | 40 - 60 |
// | **Siren** | Toggle between two static `hue` values (like Red and Blue) every N frames using a simple modulus operator on a
// counter. | 150 - 200 |
// | **Strobe** | Rapidly toggle the `val` parameter between 0 and 255 every single frame. | 40 - 50 |

}  // namespace

void start_led_rainbow(LedStrip& strip) {
  // Resets (and joins) any previous task if it was running. There can be a delay here, but it
  // ensures that we don't modify anim_data while the previous task is still running.
  anim_task.emplace();
  anim_data.strip = &strip;
  anim_data.current_effect = RainbowData{};
  anim_task->start(&anim_data, rainbow_step);
}
void start_led_breathe(uint16_t hue, LedStrip& strip) {
  anim_task.emplace();
  anim_data.strip = &strip;
  anim_data.current_effect = BreatheData{0, hue};
  anim_task->start(&anim_data, breathe_step);
}
void start_led_flash(uint8_t r, uint8_t g, uint8_t b, bool end_in_on_state, int count,
                     LedStrip& strip) {
  anim_task.emplace();
  anim_data.strip = &strip;
  anim_data.current_effect = FlashData{count * 2, !end_in_on_state, r, g, b};
  anim_task->start(&anim_data, flash_step);
}
void stop_led_effects() {
  anim_task.reset();
}

uint16_t rgb_to_hue(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t max_val = std::max({r, g, b});
  uint8_t min_val = std::min({r, g, b});
  uint8_t delta = max_val - min_val;

  // Grayscale colors (white, black, gray) have no hue
  if (delta == 0) return 0;

  int16_t hue = 0;
  if (max_val == r) {
    hue = 60 * (int16_t(g) - int16_t(b)) / delta;
  } else if (max_val == g) {
    hue = 60 * (int16_t(b) - int16_t(r)) / delta + 120;
  } else {
    hue = 60 * (int16_t(r) - int16_t(g)) / delta + 240;
  }

  // Wrap negative angles back around the 360-degree wheel
  if (hue < 0) hue += 360;

  return static_cast<uint16_t>(hue);
}

}  // namespace halpp