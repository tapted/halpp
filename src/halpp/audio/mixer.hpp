#pragma once

#include <cstddef>
#include <cstdint>

#include "espbase/esp_result.hpp"
#include "espbase/esp_task.hpp"
#include "halpp/config.hpp"

typedef void* esp_audio_render_handle_t;
typedef void* esp_audio_render_stream_handle_t;
typedef struct esp_gmf_pool* esp_gmf_pool_handle_t;

namespace halpp::audio {

class Speaker;
class Mixer;

struct ToneParams {
  Mixer* mixer = nullptr;
  int stream_id = -1;
  float frequency = 0.0f;
  uint32_t duration_ms = 0;
};

class Mixer {
 public:
  explicit constexpr Mixer(Speaker& speaker,
                           uint8_t max_streams = HAL::config::Audio::MAX_MIXER_STREAMS)
      : speaker_(speaker), max_streams_(max_streams) {}
  ~Mixer();

  const Speaker& speaker() const { return speaker_; }

  EspResult<void> begin();
  void end();

  // Stream Allocation
  // @param apply_master_volume: If false, the stream ignores master_volume_ updates
  // @param sample_rate: The sample rate of the incoming PCM data (0 = use hardware default)
  int acquire_stream(bool apply_master_volume = true, uint32_t sample_rate = 0,
                     uint8_t channels = 1, uint8_t bits_per_sample = 0);
  void release_stream(int stream_index);

  // Replaces the FreeRTOS stream buffer - Opus decoder will call this
  size_t write_chunk(int stream_index, const int16_t* samples, size_t count);

  void play_tone(float frequency, uint32_t duration_ms = UINT32_MAX);
  void stop_tone();
  void stop_and_flush_all();

  bool is_running() const { return is_running_; }
  float get_master_volume() const { return master_volume_; }
  void set_master_volume(float max_gain) { master_volume_ = max_gain; }

 private:
  struct Stream {
    esp_audio_render_stream_handle_t handle = nullptr;
    float channel_gain = 1.0f;
    float configured_gain = 1.0f;
    bool in_use = false;
  };

  Speaker& speaker_;
  const uint8_t max_streams_ = HAL::config::Audio::MAX_MIXER_STREAMS;
  bool is_running_ = false;
  float master_volume_ = 1.0f;

  esp_audio_render_handle_t renderer_ = nullptr;
  Stream* streams_ = nullptr;
  esp_gmf_pool_handle_t pool_ = nullptr;

  EspTask<ToneParams> tone_task_;
  ToneParams tone_params_{};

  Mixer(const Mixer&) = delete;
  Mixer& operator=(const Mixer&) = delete;

  static int render_write_cb(uint8_t* pcm, uint32_t len, void* ctx);
};

}  // namespace halpp::audio