# HAL++ (`halpp`) Architecture & Design Philosophy

`halpp` is a modern C++ (C++20/C++26) Hardware Abstraction Layer designed for ESP-IDF. It wraps native C API components (like `i2c_master`) into safe, ergonomic, and object-oriented paradigms.

This document outlines the strict design philosophies, idioms, and patterns that **must** be adhered to when modifying or extending this codebase.

## 1. Core Principles

* **Zero Exceptions:** ESP-IDF is traditionally compiled with exceptions disabled (`-fno-exceptions`). All error propagation is handled explicitly via the `EspResult<T>` and `EspError` monad-like wrappers.
* **Heap Allocation Avoidance:** Embedded systems have constrained memory. Dynamic allocation (`new`, `malloc`, `std::string`, `std::vector`, `std::format`) should be avoided in runtime execution paths. Use stack buffers, `std::array`, and `std::string_view`.
* **RAII & Move Semantics:** Hardware handles (like `i2c_master_dev_handle_t`) must be strictly owned by wrapper classes (e.g., `I2CDevice`). Copying hardware handles is strictly forbidden.
* **Compile-Time Evaluation:** Use `constexpr` for lookup tables, configurations, and type traits (`if constexpr`) to shift work to the compiler.

---

## 2. Error Handling Protocol (`EspResult` & `EspError`)

Native ESP-IDF returns `esp_err_t`. `halpp` uses `EspResult<T>` to bundle a return value with a potential error, and `EspError` to cleanly check and propagate those errors without verbose `if (err != ESP_OK)` boilerplate.

### Standard Propagation (Void Returns)

When calling a function that returns `EspResult<void>`, instantiate an `EspError` in the condition block:

```cpp
// Good
if (EspError err = i2c_dev_.tx({0x21})) {
    return err; // Propagates the esp_err_t seamlessly
}

// Good with Logging
if (EspError err = display.write_display()) {
    return err.log(TAG, "Failed to write display RAM");
}

```

### Value Extraction (`EspError::check`)

When a function returns `EspResult<T>` (where `T` is a handle, value, or struct), use the static `EspError::check` method with an output pointer. This utilizes rvalue references to safely `std::move` the value out of the result upon success.

```cpp
I2CDevice i2c_dev;

// Good: Automatically consumes the EspResult temporary and moves the handle
if (EspError err = EspError::check(I2CMaster::instance().add_device(0x70), &i2c_dev)) {
    return err.log(TAG, "Failed to add device to I2C bus");
}

```

---

## 3. Resource Management & RAII

Hardware interfaces in ESP-IDF require explicit creation and deletion. `halpp` uses RAII to guarantee cleanup.

* **Rule 1: Delete Copy Constructors.** A hardware handle represents a physical wire/device. It cannot be duplicated in memory.
* **Rule 2: Implement Move Semantics.** Implement `noexcept` move constructors and move-assignment operators to transfer ownership securely.
* **Rule 3: Clean up in Destructor.** The destructor must release the IDF handle if it is currently owned.

**Example (from `I2CDevice`):**

```cpp
I2CDevice(const I2CDevice&) = delete;
I2CDevice& operator=(const I2CDevice&) = delete;

I2CDevice(I2CDevice&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
}

```

---

## 4. Initialization Patterns

### The Two-Stage Initialization Rule

Constructors **cannot return errors**. Therefore, no hardware transactions (like I2C bus writes) may occur inside a constructor.

1. **Stage 1 (Construction):** Inject dependencies (e.g., pass the `I2CDevice` via `std::move`).
2. **Stage 2 (Hardware Init):** Expose a `begin()` or `init()` method that performs hardware setup and returns an `EspResult<void>`.

### The "Default Instance" Idiom

Many hardware components (like a specific RTC or a primary display) are often the *only* component of their type on a board. To support multi-device chaining without forcing users to pass objects everywhere, implement the "Default Instance" pattern.

* Provide standard constructors for multi-device support.
* Provide a static `default_instance()` method returning a singleton.
* Provide static `init_default()` and `deinit_default()` wrappers.

**Example:**

```cpp
class I2C7Seg {
public:
  // Standard Multi-Device Constructor
  explicit I2C7Seg(I2CDevice device = I2CDevice{}) : i2c_dev_(std::move(device)) {}
  EspResult<void> begin();

  // Single-Device Default Optimization
  static I2C7Seg& default_instance() {
    static I2C7Seg inst;
    return inst;
  }
  static EspResult<void> init_default(uint8_t i2c_address = 0x70);
};

```

---

## 5. Modern C++ Embedded Idioms

### Template Array Deduction

When sending raw byte buffers to standard protocols like I2C, use template size deduction to prevent buffer overrun errors and avoid explicitly passing `size_t` arguments.

```cpp
// Allows calling: i2c_dev.tx({0x21, 0x05, 0x00});
template <size_t N>
EspResult<void> tx(const uint8_t (&data)[N], int timeout_ms = 1000) {
    return i2c_master_transmit(handle_, data, N, timeout_ms);
}

```

### Formatting Without Allocations

Avoid `std::format` or `std::string` for text processing, as they allocate memory on the heap.
Instead, use small stack buffers, `snprintf`, and `std::string_view` for parsing.

```cpp
// Good: Type-safe, dynamic precision, zero heap allocations
void print_float(double n, uint8_t frac_digits = 2) {
    char buf[32]; 
    snprintf(buf, sizeof(buf), "%.*f", frac_digits, n);
    print(buf); // print() expects std::string_view
}

```

### Table Lookups

Hardware mapping tables (like ASCII to 7-Segment displays) should be declared as `static constexpr std::array<uint8_t, N>`. This ensures the array is baked directly into the `.rodata` segment of the flash memory.