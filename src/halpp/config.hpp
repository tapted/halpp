/**
 * @file config.hpp
 * @brief Zero-overhead, sparsely-overridable configuration for halpp.
 */

#pragma once

#if __has_include("hal/board.hpp")
#include "hal/board.hpp"
namespace HAL {
// Alias to the user's custom struct
using config = HAL::board::config;
}  // namespace HAL
#else
#include "halpp/config_defaults.hpp"
namespace HAL {
// No board file found? Alias directly to the framework defaults.
using config = HAL::detail::Defaults;
}  // namespace HAL
#endif