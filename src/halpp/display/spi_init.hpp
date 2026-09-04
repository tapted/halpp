#pragma once

#include <driver/spi_common.h>
#include <esp_lcd_io_spi.h>

#include "espbase/esp_result.hpp"
#include "halpp/config.hpp"
#include "halpp/display/display.hpp"

namespace halpp {

template <typename T = void>
EspResult<Display::Config> init_spi_display(Display* instance) {
  esp_lcd_panel_io_spi_config_t::esp_lcd_spi_flags_t flags = {};
  spi_bus_config_t bus_config = {};
  bus_config.sclk_io_num = config::SpiBus::PIN_SERIAL_CLOCK;
  bus_config.max_transfer_sz = 0;  // Use default max transfer size (4092 bytes for DMA)
  bus_config.flags = SPICOMMON_BUSFLAG_MASTER;
  bus_config.intr_flags = 0;
  if constexpr (config::SpiBus::PIN_QSPI_SDA_0 != GPIO_NUM_NC) {
    bus_config.data0_io_num = config::SpiBus::PIN_QSPI_SDA_0;
    bus_config.data1_io_num = config::SpiBus::PIN_QSPI_SDA_1;
    bus_config.data2_io_num = config::SpiBus::PIN_QSPI_SDA_2;
    bus_config.data3_io_num = config::SpiBus::PIN_QSPI_SDA_3;
    flags.quad_mode = 1;  // QSPI mode
  } else if constexpr (config::SpiBus::PIN_MOSI != GPIO_NUM_NC) {
    bus_config.mosi_io_num = config::SpiBus::PIN_MOSI;
    bus_config.miso_io_num = config::SpiBus::PIN_MISO;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
  } else {
    static_assert(false, "No SPI data pins defined in halpp::config::SpiBus");
  }
  bus_config.data4_io_num = -1;
  bus_config.data5_io_num = -1;
  bus_config.data6_io_num = -1;
  bus_config.data7_io_num = -1;

  if (EspError err = spi_bus_initialize(config::SpiBus::SPI_HOST, &bus_config, SPI_DMA_CH_AUTO)) {
    return err.log("halpp::init_spi_display", "Failed to initialize SPI bus");
  }

  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = config::SpiBus::PIN_CHIP_SELECT,
      .dc_gpio_num = config::Display::PIN_DATA_COMMAND,
      .spi_mode = 0,
      .pclk_hz = config::SpiBus::SPI_CLK_WRITE_HZ,
      .trans_queue_depth = 10,
      .on_color_trans_done = Display::on_color_trans_done,
      .user_ctx = instance,
      .lcd_cmd_bits = config::Display::LCD_COMMAND_BITS,
      .lcd_param_bits = config::Display::LCD_PARAM_BITS,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .flags = flags,
  };

  esp_lcd_panel_io_handle_t io_handle = nullptr;
  if (EspError err = esp_lcd_new_panel_io_spi(config::SpiBus::SPI_HOST, &io_config, &io_handle)) {
    return err.log("halpp::init_spi_display", "Failed to create SPI IO handle");
  }

  // Config for injecting into bas class.
  return Display::Config{
      .width = config::Display::WIDTH,
      .height = config::Display::HEIGHT,
      .bits_per_pixel = config::Display::BITS_PER_PIXEL,
      .io_handle = io_handle,
      .owns_io_handle = true,
  };
}

}  // namespace halpp