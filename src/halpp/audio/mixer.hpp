#pragma once

#include <cstddef>
#include <cstdint>

#include "espbase/esp_result.hpp"
#include "halpp/config.hpp"

typedef void* esp_audio_render_handle_t;
typedef void* esp_audio_render_stream_handle_t;
typedef struct esp_gmf_pool* esp_gmf_pool_handle_t;

namespace halpp::audio {

class Speaker;

class Mixer {
 public:
  explicit constexpr Mixer(Speaker& speaker,
                           uint8_t max_streams = HAL::config::Audio::MAX_MIXER_STREAMS)
      : speaker_(speaker), max_streams_(max_streams) {}
  ~Mixer();

  EspResult<void> begin();
  void end();

  // Replaces the FreeRTOS stream buffer - Opus decoder will call this
  size_t write_chunk(const int16_t* samples, size_t count);

  bool is_running() const { return is_running_; }
  float get_master_volume() const { return master_volume_; }
  void set_master_volume(float max_gain) {
    if (max_gain != master_volume_) {
      master_volume_ = max_gain;
      on_master_volume_change();
    }
  }

 private:
  struct Stream {
    esp_audio_render_stream_handle_t handle = nullptr;
    float channel_gain = 1.0f;
    float configured_gain = 1.0f;
  };

  Speaker& speaker_;
  const uint8_t max_streams_ = HAL::config::Audio::MAX_MIXER_STREAMS;
  bool is_running_ = false;
  float master_volume_ = 1.0f;

  esp_audio_render_handle_t renderer_ = nullptr;
  Stream* streams_ = nullptr;
  esp_gmf_pool_handle_t pool_ = nullptr;

  Mixer(const Mixer&) = delete;
  Mixer& operator=(const Mixer&) = delete;

  void on_master_volume_change();
  static int render_write_cb(uint8_t* pcm, uint32_t len, void* ctx);
};

}  // namespace halpp::audio