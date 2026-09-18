#pragma once

#include <cassert>
#include <mutex>
#include <optional>
#include <utility>

#include "espbase/esp_result.hpp"
#include "espbase/shutdown_registry.hpp"

namespace halpp {

/**
 * @brief CRTP Base class to standardize thread-safe "Default Instance" singletons.
 * @tparam T The derived class (e.g., class LedStrip : public DefaultInstance<LedStrip>)
 */
template <typename T>
class DefaultInstance {
 public:
  // The instance (unguarded) - caller must ensure it is initialized before use.
  static T& default_instance() { return *default_optional(); }

  // Threadsafe initialization check for sites doing setup/teardown.
  static bool is_default_initialized() {
    std::lock_guard<std::mutex> lock(default_mutex());
    return default_optional().has_value();
  }

  // Default RAII cleanup. Derived classes can shadow this if they need to return
  // hardware error codes during shutdown (e.g., returning the result of a reset).
  static EspResult<void> deinit_default() {
    std::lock_guard<std::mutex> lock(default_mutex());
    default_optional().reset();
    return ESP_OK;
  }

 protected:
  constexpr DefaultInstance() = default;

  // Path A: Factory Initialization (e.g., LedStrip::create_rmt -> std::move)
  static EspResult<void> set_default_instance(T&& obj) {
    std::lock_guard<std::mutex> lock(default_mutex());
    if (default_optional()) return ESP_ERR_INVALID_STATE;
    ShutdownRegistry::register_fn(&T::deinit_default);
    default_optional() = std::move(obj);
    return ESP_OK;
  }

  // Path B: Emplace & Initialize (e.g., Display, I2C7Seg)
  template <typename... Args>
  static T& emplace_default_instance(Args&&... args) {
    std::lock_guard<std::mutex> lock(default_mutex());
    if (!default_optional()) {
      default_optional().emplace(std::forward<Args>(args)...);
      ShutdownRegistry::register_fn(&T::deinit_default);
    }
    return *default_optional();
  }

  // Expose the raw optional and mutex for highly custom derived class logic
  static std::optional<T>& default_optional() {
    static constinit std::optional<T> instance_opt;
    return instance_opt;
  }
  static std::mutex& default_mutex() {
#ifdef __clang__
    static std::mutex m;
#else
    static constinit std::mutex m;
#endif
    return m;
  }

 private:
  // Disable copy construction and assignment to enforce singleton behavior.
  DefaultInstance(const DefaultInstance&) = delete;
  DefaultInstance& operator=(const DefaultInstance&) = delete;
};

}  // namespace halpp