cmake_minimum_required(VERSION 3.24)
project(halpp_tests CXX C)

# Enforce modern C++
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Reuse the googletest sources already fetched by deps/espbase/tests to avoid
# a redundant network clone. Falls back to a git fetch if the path is absent.
set(GTEST_LOCAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../espbase/tests/build/_deps/googletest-src")
if(EXISTS "${GTEST_LOCAL_SRC}/CMakeLists.txt")
    FetchContent_Declare(googletest SOURCE_DIR "${GTEST_LOCAL_SRC}")
else()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
endif()
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Unit Tests ---
add_executable(halpp_tests
    halpp/display/custom_rgb565_swap_test.cpp
)

target_include_directories(halpp_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(halpp_tests PRIVATE
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(halpp_tests)

# --- Xtensa ESP32-S3 Assembly Analysis Target ---
# Compiles custom_rgb565_swap for the target architecture and dumps annotated
# assembly for static analysis. Does NOT run on the host machine.
#
# Prerequisites: Espressif xtensa-esp-elf toolchain must be installed.
# Usage: cmake --build build --target asm_analysis
#
# Key flags extracted from the ESP-IDF build system (compile_commands.json):
#   -mlongcalls          : Required for Xtensa call8/call0 ABI
#   -Os                  : Optimize for size (matches production build)
#   -std=gnu++26         : Match production C++ standard
#   -fno-exceptions      : Match production flags
#   -fno-rtti            : Match production flags
#   -mno-target-align    : Match production flags
#   -fno-jump-tables     : Match production flags (prevents indirect branches)
#   -fno-tree-switch-conversion : Match production flags
set(XTENSA_CXX "C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++.exe")
set(XTENSA_OBJDUMP "C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe")
set(ASM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/halpp/display/asm_analysis.cpp")
set(ASM_OBJ "${CMAKE_CURRENT_BINARY_DIR}/custom_rgb565_swap_xtensa.o")
set(ASM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/custom_rgb565_swap_xtensa.s")

add_custom_target(asm_analysis
    COMMAND ${XTENSA_CXX}
            -std=gnu++26
            -Os
            -ffunction-sections
            -fdata-sections
            -fno-exceptions
            -fno-rtti
            -fno-jump-tables
            -fno-tree-switch-conversion
            -mno-target-align
            -mlongcalls
            -I${CMAKE_CURRENT_SOURCE_DIR}/../src
            -S
            -o ${ASM_OUTPUT}
            ${ASM_SOURCE}
    COMMAND ${CMAKE_COMMAND} -E echo "Assembly written to: ${ASM_OUTPUT}"
    COMMENT "Compiling custom_rgb565_swap for Xtensa ESP32-S3 (static analysis only, not runnable on host)"
    USES_TERMINAL
)
