#pragma once

#include "espbase/esp_result.hpp"
#include "halpp/display/display.hpp"

namespace halpp {

EspResult<Display::Config> init_i2c_display(Display* instance);

}  // namespace halpp