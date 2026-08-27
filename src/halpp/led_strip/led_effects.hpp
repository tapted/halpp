#pragma once

#include "halpp/led_strip/led_strip.hpp"

namespace halpp {
class LedStrip;

void start_led_rainbow(LedStrip& strip = LedStrip::default_instance());
void start_led_breathe(uint16_t hue, LedStrip& strip = LedStrip::default_instance());
void start_led_flash(uint8_t r, uint8_t g, uint8_t b, bool end_in_on_state = false, int count = 4, LedStrip& strip = LedStrip::default_instance());
void stop_led_effects();

uint16_t rgb_to_hue(uint8_t r, uint8_t g, uint8_t b);

}  // namespace halpp