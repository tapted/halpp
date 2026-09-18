#pragma once

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
  // Destructors usually just call reset(), but lose the return value. This allows deinit_default
  // to still report errors if needed.
  EspResult<> reset() { return ESP_OK; }

  template <typename... Args>
  static EspResult<> init_default(Args&&... args) {
    std::lock_guard<std::mutex> lock(default_mutex());
    T& instance = emplace_default_instance(std::forward<Args>(args)...);

    // Try to call the instance's init() method if it exists. Client classes must either have a per
    // instance init(), or shadow `init_default` with their own implementation. E.g., to use
    // set_default_instance() after creating the instance.
    return instance.init();
  }

  // The instance (unguarded) - caller must ensure it is initialized before use.
  static T& default_instance() { return *default_optional(); }

  // Threadsafe initialization check for sites doing setup/teardown.
  static bool is_default_initialized() {
    std::lock_guard<std::mutex> lock(default_mutex());
    return default_optional().has_value();
  }

  // Default RAII cleanup. Derived classes can shadow this if they need to return
  // hardware error codes during shutdown (e.g., returning the result of a reset).
  static EspResult<> deinit_default() {
    std::lock_guard<std::mutex> lock(default_mutex());
    EspResult<> result = ESP_OK;
    if (default_optional()) {
      result = default_optional()->reset();
      default_optional().reset();
    }
    return result;
  }

 protected:
  constexpr DefaultInstance() = default;

  // Path A: Factory Initialization (e.g., LedStrip::create_rmt -> std::move)
  static EspResult<> set_default_instance(T&& obj) {
    std::lock_guard<std::mutex> lock(default_mutex());
    if (default_optional()) return ESP_ERR_INVALID_STATE;
    ShutdownRegistry::register_fn(
        [] { T::deinit_default().log_error("default_instance", __PRETTY_FUNCTION__); });
    default_optional() = std::move(obj);
    return ESP_OK;
  }

  static std::optional<T>& default_optional() {
    static constinit std::optional<T> instance_opt;
    return instance_opt;
  }
  static std::mutex& default_mutex() {
    static std::mutex m;
    return m;
  }

 private:
  // Path B: Emplace & Initialize (e.g., Display, I2C7Seg). Lock must be held.
  template <typename... Args>
  static T& emplace_default_instance(Args&&... args) {
    if (!default_optional()) {
      default_optional().emplace(std::forward<Args>(args)...);
      ShutdownRegistry::register_fn(
          [] { T::deinit_default().log_error("default_instance", __PRETTY_FUNCTION__); });
    }
    return *default_optional();
  }

  // Disable copy construction and assignment to enforce singleton behavior.
  DefaultInstance(const DefaultInstance&) = delete;
  DefaultInstance& operator=(const DefaultInstance&) = delete;
};

}  // namespace halpp