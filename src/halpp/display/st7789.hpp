#pragma once

#include <hal/spi_types.h>
#include <optional>

#include "halpp/display/display.hpp"
#include "halpp/config.hpp"

namespace halpp {

class St7789 : public Display {
 public:
  using Display::Display;

  static St7789& default_instance() {
    static std::optional<St7789> inst;
    if (!inst) inst.emplace();
    return *inst;
  }

  // Initializes the SPI IO and links it to the Display base class
  static EspResult<void> init_default_spi(spi_host_device_t spi_host = config::SpiBus::SPI_HOST);
  static EspResult<void> deinit_default() { return default_instance().reset(); }

  // Initializes the ST7789 panel hardware
  EspResult<void> begin();
};

}  // namespace halpp