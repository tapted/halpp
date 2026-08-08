#pragma once

#include <esp_err.h>
#include <mutex>

struct i2s_channel_obj_t;
typedef struct i2s_channel_obj_t* i2s_chan_handle_t;

namespace halpp::audio {

class I2SMaster {
 public:
  static I2SMaster& instance() {
    static I2SMaster inst;
    return inst;
  }

  // Returns a handle and increments ref count
  esp_err_t retain_tx(i2s_chan_handle_t* out_handle);
  esp_err_t retain_rx(i2s_chan_handle_t* out_handle);

  // Decrements ref count and tears down if 0
  void release_tx();
  void release_rx();

 private:
  I2SMaster();
  ~I2SMaster();

  void _deinit_i2s();

  std::mutex _mutex;
  int _tx_refs = 0;
  int _rx_refs = 0;

  bool _i2s_allocated = false;
  bool _tx_initialized = false;
  bool _rx_initialized = false;

  i2s_chan_handle_t _tx_handle = nullptr;
  i2s_chan_handle_t _rx_handle = nullptr;
};

}  // namespace halpp::audio