#include "halpp/audio/mixer.hpp"

#include <cmath>
#include <esp_audio_render.h>
#include <esp_gmf_pool.h>
#include <esp_log.h>

#include "halpp/audio/speaker.hpp"

static constexpr char TAG[] = "GMF_MIXER";

namespace halpp::audio {

Mixer::~Mixer() {
  end();
  delete[] streams_;
}

// esp_audio_render_write_cb_t
int Mixer::render_write_cb(uint8_t* pcm, uint32_t len, void* ctx) {
  EspResult<size_t> res = static_cast<Speaker*>(ctx)->write(pcm, len);
  return res ? 0 : -1;  // 0 on success, "others" mean failure.
}

EspResult<> Mixer::begin() {
  if (is_running_) return ESP_OK;
  if (!streams_) {
    streams_ = new Stream[max_streams_];
  }

  if (EspError err = speaker_.start_tx()) {
    return err.log(TAG, "Failed to start speaker");
  }

  // 1. Initialize the GMF Object Pool
  esp_gmf_pool_init(&pool_);

  // 2. Configure the Renderer Output
  esp_audio_render_cfg_t cfg = {
      .max_stream_num = max_streams_,
      .out_writer = render_write_cb,
      .out_ctx = &speaker_,
      .out_sample_info =
          {
              .sample_rate = speaker_.config().sample_rate,
              .bits_per_sample = static_cast<uint8_t>(speaker_.config().bits_per_sample),
              .channel = 1,
          },
      .pool = pool_,
      .process_period{},
      .process_buf_align{}};

  esp_audio_render_create(&cfg, &renderer_);

  // 3. Configure the Input Stream
  esp_audio_render_sample_info_t in_info = {
      .sample_rate = speaker_.config().sample_rate,
      .bits_per_sample = static_cast<uint8_t>(speaker_.config().bits_per_sample),
      .channel = 1,
  };

  for (int i = 0; i < max_streams_; ++i) {
    esp_audio_render_stream_get(renderer_, i, &streams_[i].handle);
    esp_audio_render_stream_open(streams_[i].handle, &in_info);
  }

  is_running_ = true;
  ESP_LOGI(TAG, "GMF Audio Render started successfully with %d streams.", (int)max_streams_);
  return ESP_OK;
}

void Mixer::end() {
  if (!is_running_) return;

  if (streams_) {
    for (int i = 0; i < max_streams_; ++i) {
      if (streams_[i].handle) {
        esp_audio_render_stream_close(streams_[i].handle);
        streams_[i].handle = nullptr;
      }
    }
  }

  if (renderer_) {
    esp_audio_render_destroy(renderer_);
    renderer_ = nullptr;
  }

  if (pool_) {
    esp_gmf_pool_deinit(pool_);
    pool_ = nullptr;
  }

  is_running_ = false;
}

size_t Mixer::write_chunk(const int16_t* samples, size_t count) {
  if (!is_running_ || !streams_ || count == 0) return 0;

  size_t bytes_to_write = count * sizeof(int16_t);

  esp_audio_render_err_t err =
      esp_audio_render_stream_write(streams_[0].handle, (uint8_t*)samples, bytes_to_write);

  if (err != ESP_AUDIO_RENDER_ERR_OK) {
    ESP_LOGE(TAG, "Renderer write failed: %d", err);
    return 0;
  }

  return count;
}

void Mixer::on_master_volume_change() {
  if (!is_running_ || !streams_) return;

  for (int i = 0; i < max_streams_; ++i) {
    if (streams_[i].handle) {
      float target_gain = streams_[i].channel_gain * master_volume_;
      if (std::fabs(streams_[i].configured_gain - target_gain) < 0.01f) continue;

      streams_[i].configured_gain = target_gain;
      esp_audio_render_mixer_gain_t mixer_gain = {
          .initial_gain = target_gain, .target_gain = target_gain, .transition_time = 10};
      esp_audio_render_stream_set_mixer_gain(streams_[i].handle, &mixer_gain);
    }
  }
}

}  // namespace halpp::audio