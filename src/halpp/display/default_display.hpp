#pragma once

#include "halpp/config.hpp"
#include "halpp/display/display.hpp"

namespace halpp {

// Helper to get the concrete type of the default display. This can't be in display.hpp, because
// config.hpp may transitively include display.hpp to provide a DisplayType that inherits from
// halpp::Display.
inline config::Display::DisplayType& default_display() {
  return static_cast<config::Display::DisplayType&>(Display::instance());
}

}  // namespace halpp