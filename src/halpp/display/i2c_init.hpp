#pragma once

#include <esp_lcd_io_i2c.h>

#include "espbase/esp_result.hpp"
#include "halpp/config.hpp"
#include "halpp/display/display.hpp"
#include "halpp/i2c/i2c_master.hpp"

namespace halpp {

template <typename T = void>
EspResult<Display::Config> init_i2c_display(Display* instance) {
  // 1. Configure the I2C IO layer specifics for the SSD1306
  esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = config::Display::I2C_ADDRESS,
      .scl_speed_hz = config::I2CConfig::CLK_SPEED,  // 400 kHz is the maximum for SSD1306
      .control_phase_bytes = config::Display::I2C_CONTROL_PHASE_BYTES,  // 1 byte for SSD1306
      // Crucial: Tells the IO layer where the Data/Command bit lives
      .dc_bit_offset = config::Display::I2C_DC_BIT_OFFSET,
      .lcd_cmd_bits = config::Display::LCD_COMMAND_BITS,
      .lcd_param_bits = config::Display::LCD_PARAM_BITS,
      .on_color_trans_done = Display::on_color_trans_done,
      .user_ctx = instance,
      .flags =
          {
              .dc_low_on_data = 0,
              .disable_control_phase = 0,
          },
  };

  esp_lcd_panel_io_handle_t io_handle = nullptr;

  // Link the display IO to halpp's modern I2C Master bus
  if (EspError err = esp_lcd_new_panel_io_i2c(I2CMaster::instance().get_bus_handle(), &io_config,
                                              &io_handle)) {
    return err.log("halpp::init_i2c_display", "Failed to create I2C IO handle");
  }

  // Inject into the base class configuration. Note the bits_per_pixel!
  return Display::Config{
      .width = config::Display::WIDTH,
      .height = config::Display::HEIGHT,
      .bits_per_pixel = config::Display::BITS_PER_PIXEL,
      .io_handle = io_handle,
      .owns_io_handle = true,
  };
}

}  // namespace halpp