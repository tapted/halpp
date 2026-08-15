/**
 * @file config.hpp
 * @brief Zero-overhead, sparsely-overridable configuration for halpp.
 */

#pragma once

#if __has_include("hal/board.hpp")
#include "hal/board.hpp"
namespace halpp {
// Alias to the user's custom struct
using config = halpp::board::config;
}  // namespace halpp
#else
#include "halpp/config_defaults.hpp"
namespace halpp {
// No board file found? Alias directly to the framework defaults.
using config = halpp::detail::Defaults;
}  // namespace halpp
#endif