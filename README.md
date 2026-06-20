# HAL++ (`halpp`)

`halpp` is a modern, object-oriented C++ (C++20/C++26) Hardware Abstraction Layer designed specifically for the ESP-IDF framework. It wraps native ESP-IDF C API components (such as the `i2c_master` driver, RTC modules, and display drivers) into safe, ergonomic, and highly reliable C++ classes.

## Project Goals

The primary goal of `halpp` is to bring modern C++ paradigms to ESP32 embedded development without sacrificing performance or bloating firmware size.

* **Safe Error Handling:** Replaces verbose `esp_err_t` checking with a monadic `EspResult<T>` and `EspError` pattern, completely avoiding C++ exceptions.
* **Deterministic Memory:** Strictly avoids dynamic memory allocation (`new`, `malloc`, `std::string`, `std::vector`) in runtime execution paths to prevent heap fragmentation.
* **RAII Resource Management:** Hardware handles (like I2C bus devices) are strictly managed by object lifecycles, ensuring proper initialization and automatic cleanup.
* **Zero Overhead:** Leverages `constexpr`, template size deduction, and `string_view` to shift computational work to the compiler wherever possible.

📚 **For a deep dive into the coding idioms, architectural rules, and contribution guidelines, please read the [Architecture & Design Philosophy Guide](src/README.md).**

## Dependencies

`halpp` is designed to be lightweight but relies on a foundational utility library for its error-handling types:

* **ESP-IDF** (v5.x / v6.x compatible)
* **`espbase`**: Provides the `EspResult` and `EspError` implementations. You must include `espbase` in your project alongside `halpp`.

## Integration Guide

The recommended way to integrate `halpp` into your ESP-IDF project is via git submodules and CMake source inclusion.

### 1. Add Submodules

Add both `halpp` and `espbase` to a `deps/` directory in your project root:

```bash
mkdir deps
git submodule add https://github.com/tapted/halpp.git deps/halpp
git submodule add https://github.com/tapted/espbase.git deps/espbase
git submodule update --init --recursive

```

### 2. Update your component `CMakeLists.txt`

In your main application component (e.g., `main/CMakeLists.txt`), include the source files and header directories from the submodules.

Here is a standard integration example:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Glob the source files from the submodules
file(GLOB_RECURSE DEPS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/espbase/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/halpp/src/*.cpp
)

# 2. Glob your own application sources
file(GLOB_RECURSE MAIN_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp
)

set(PROJECT_SOURCES ${MAIN_SRCS} ${DEPS_SRCS})

# 3. Register the component
idf_component_register(SRCS ${PROJECT_SOURCES} INCLUDE_DIRS ".")

# 4. Expose the include directories to your target
target_include_directories(${COMPONENT_LIB} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/espbase/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/halpp/src
)

```

Make sure your top-level project `CMakeLists.txt` initializes the project normally:

```cmake
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(your_project_name)

```

## Directory Structure

* `/src/halpp/` - The core HAL implementation headers and source files.
* `/i2c/` - Safe RAII wrappers for the I2C Master bus and devices.
* `/segmented/` - Drivers for segmented displays (e.g., HT16K33 Adafruit Backpacks).


* `/src/README.md` - The detailed styling and architectural guide for the library.

## License

Licensed under the Apache License 2.0.