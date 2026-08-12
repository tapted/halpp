#include "halpp/audio/mixer.hpp"

#include <cmath>
#include <esp_audio_render.h>
#include <esp_gmf_bit_cvt.h>
#include <esp_gmf_ch_cvt.h>
#include <esp_gmf_pool.h>
#include <esp_gmf_rate_cvt.h>
#include <esp_log.h>
#include <numbers>

#include "halpp/audio/speaker.hpp"

static constexpr char TAG[] = "GMF_MIXER";
static constexpr uint32_t TONE_SAMPLE_RATE = 12000;  // 12kHz sample rate for tone generation

namespace halpp::audio {

static int create_default_pool(esp_gmf_pool_handle_t* pool) {
  *pool = NULL;
  if (esp_gmf_pool_init(pool) != ESP_GMF_ERR_OK) {
    return -1;
  }

  esp_gmf_element_handle_t el = NULL;
  esp_ae_ch_cvt_cfg_t ch_cvt_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
  esp_gmf_ch_cvt_init(&ch_cvt_cfg, &el);
  esp_gmf_pool_register_element(*pool, el, NULL);

  esp_ae_bit_cvt_cfg_t bit_cvt_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
  esp_gmf_bit_cvt_init(&bit_cvt_cfg, &el);
  esp_gmf_pool_register_element(*pool, el, NULL);

  esp_ae_rate_cvt_cfg_t rate_cvt_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
  esp_gmf_rate_cvt_init(&rate_cvt_cfg, &el);
  esp_gmf_pool_register_element(*pool, el, NULL);

  // esp_ae_alc_cfg_t alc_cfg = DEFAULT_ESP_GMF_ALC_CONFIG();
  // esp_gmf_alc_init(&alc_cfg, &el);
  // esp_gmf_pool_register_element(*pool, el, NULL);

  // esp_ae_sonic_cfg_t sonic_cfg = DEFAULT_ESP_GMF_SONIC_CONFIG();
  // esp_gmf_sonic_init(&sonic_cfg, &el);
  // esp_gmf_pool_register_element(*pool, el, NULL);
  return 0;
}

static void tone_generator_task_16bit(EspTask<ToneParams>& task) {
  constexpr float pi = static_cast<float>(std::numbers::pi);
  constexpr uint32_t sample_rate = TONE_SAMPLE_RATE;

  ToneParams* params = task.data();
  Mixer* mixer = params->mixer;

  const bool infinite = (params->duration_ms == UINT32_MAX);
  const uint32_t total_samples = infinite ? UINT32_MAX : (params->duration_ms * sample_rate) / 1000;

  // Calculate the chunk size for a 20ms GMF processing period
  constexpr uint32_t chunk_samples = (sample_rate * 20) / 1000;
  int16_t buffer[chunk_samples];

  float phase = 0.0f;
  const float phase_increment = (2.0f * pi * params->frequency) / static_cast<float>(sample_rate);

  uint32_t samples_generated = 0;

  while (!task.is_stop_requested() && mixer->is_running() &&
         (infinite || samples_generated < total_samples)) {
    uint32_t samples_this_chunk = chunk_samples;
    if (!infinite) {
      samples_this_chunk = std::min<uint32_t>(chunk_samples, total_samples - samples_generated);
    }
    // Grab the live volume knob setting at the start of this 20ms chunk
    float current_vol = mixer->get_master_volume();

    for (uint32_t i = 0; i < samples_this_chunk; ++i) {
      // Use single-precision sinf for hardware FPU acceleration
      buffer[i] = static_cast<int16_t>(32767.0f * current_vol * std::sinf(phase));

      phase += phase_increment;
      if (phase >= 2.0f * pi) {
        phase -= 2.0f * pi;
      }
    }

    // Pushing blocks if the renderer is full, keeping pace with I2S
    size_t written = mixer->write_chunk(params->stream_id, buffer, samples_this_chunk);

    // Break early if writing fails (e.g. pipeline was torn down)
    if (written == 0) break;

    samples_generated += samples_this_chunk;
  }

  // End of task scope. Mark stream as free.
  mixer->release_stream(params->stream_id);
}

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

  if (create_default_pool(&pool_) != 0) {
    speaker_.end_tx();
    return EspError(ESP_FAIL).log(TAG, "Failed to create GMF pool");
  }

  // 2. Configure the Renderer Output
  esp_audio_render_cfg_t cfg = {
      .max_stream_num = max_streams_,
      .out_writer = render_write_cb,
      .out_ctx = &speaker_,
      .out_sample_info =
          {
              .sample_rate = speaker_.config().sample_rate,
              .bits_per_sample = static_cast<uint8_t>(speaker_.config().bits_per_sample),
              .channel = speaker_.config().is_stereo ? uint8_t{2} : uint8_t{1},
          },
      .pool = pool_,
      .process_period{},
      .process_buf_align{}};

  esp_audio_render_create(&cfg, &renderer_);

  for (int i = 0; i < max_streams_; ++i) {
    esp_audio_render_stream_get(renderer_, i, &streams_[i].handle);
  }

  is_running_ = true;
  ESP_LOGI(TAG, "GMF Audio Render started successfully with %d streams.", (int)max_streams_);
  return ESP_OK;
}

void Mixer::end() {
  if (!is_running_) return;

  stop_tone();

  if (streams_) {
    for (int i = 0; i < max_streams_; ++i) {
      if (streams_[i].handle && streams_[i].in_use) {
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

int Mixer::acquire_stream(bool apply_master_volume, uint32_t sample_rate, uint8_t channels,
                          uint8_t bits_per_sample) {
  if (!is_running_ || !streams_) return -1;

  for (int i = 0; i < max_streams_; ++i) {
    if (!streams_[i].in_use && streams_[i].handle) {
      streams_[i].in_use = true;

      // Calculate gain based on whether this stream tracks the master volume
      float target_gain = streams_[i].channel_gain * (apply_master_volume ? master_volume_ : 1.0f);
      streams_[i].configured_gain = target_gain;

      esp_audio_render_mixer_gain_t mixer_gain = {
          .initial_gain = target_gain,
          .target_gain = target_gain,
          .transition_time = 10,
      };
      esp_audio_render_stream_set_mixer_gain(streams_[i].handle, &mixer_gain);

      // Resolve the actual stream parameters (fallback to speaker hardware config if 0)
      uint32_t actual_sample_rate = (sample_rate > 0) ? sample_rate : speaker_.config().sample_rate;
      uint8_t actual_bits_per_sample =
          (bits_per_sample > 0) ? bits_per_sample
                                : static_cast<uint8_t>(speaker_.config().bits_per_sample);
      uint8_t actual_channels = (channels > 0) ? channels : (speaker_.config().is_stereo ? 2 : 1);

      esp_audio_render_sample_info_t in_info = {
          .sample_rate = actual_sample_rate,
          .bits_per_sample = actual_bits_per_sample,
          .channel = actual_channels,
      };

      // Open the stream with the exact format of your source data!
      esp_audio_render_err_t err = esp_audio_render_stream_open(streams_[i].handle, &in_info);
      if (err == ESP_AUDIO_RENDER_ERR_OK) {
        return i;
      } else {
        ESP_LOGE(TAG, "Failed to open stream %d: %d", i, err);
        streams_[i].in_use = false;  // Rollback on failure
      }
    }
  }
  return -1;
}

void Mixer::release_stream(int stream_index) {
  if (!is_running_ || !streams_ || stream_index < 0 || stream_index >= max_streams_) return;

  if (streams_[stream_index].handle && streams_[stream_index].in_use) {
    esp_audio_render_stream_flush(streams_[stream_index].handle);
    esp_audio_render_stream_close(streams_[stream_index].handle);
    streams_[stream_index].in_use = false;
  }
}

size_t Mixer::write_chunk(int stream_index, const int16_t* samples, size_t count) {
  if (!is_running_ || !streams_ || count == 0) return 0;

  size_t bytes_to_write = count * sizeof(int16_t);
  bool autorelease_stream = false;
  if (stream_index < 0) {
    stream_index = acquire_stream(true, TONE_SAMPLE_RATE);
    autorelease_stream = true;
  }
  if (stream_index < 0 || stream_index >= max_streams_ || !streams_[stream_index].handle) {
    return 0;
  }

  esp_audio_render_err_t err = esp_audio_render_stream_write(streams_[stream_index].handle,
                                                             (uint8_t*)samples, bytes_to_write);

  if (autorelease_stream) {
    release_stream(stream_index);
  }

  if (err != ESP_AUDIO_RENDER_ERR_OK) {
    ESP_LOGE(TAG, "Renderer write failed: %d", err);
    return 0;
  }

  return count;
}

void Mixer::play_tone(float frequency, uint32_t duration_ms) {
  if (!is_running_) return;
  if (speaker_.config().bits_per_sample != 16) {
    ESP_LOGW(TAG, "Tone generation only supports 16-bit samples. Current config: %d",
             speaker_.config().bits_per_sample);
    return;
  }

  stop_tone();

  tone_params_.mixer = this;
  tone_params_.frequency = frequency;
  tone_params_.duration_ms = duration_ms;
  tone_params_.stream_id = acquire_stream(false, TONE_SAMPLE_RATE);

  if (tone_params_.stream_id < 0) {
    ESP_LOGW(TAG, "Failed to play tone: No free mixer streams available.");
    return;
  }

  if (EspError err = tone_task_.start({.name = "tone", .stack_size = 4096}, &tone_params_,
                                      tone_generator_task_16bit)) {
    err.log(TAG, "Failed to start tone generation task");
    release_stream(tone_params_.stream_id);
  }
}

void Mixer::stop_tone() {
  tone_task_.reset();
}

void Mixer::stop_and_flush_all() {
  stop_tone();
  if (!is_running_ || !streams_) return;

  for (int i = 0; i < max_streams_; ++i) {
    if (streams_[i].handle) {
      esp_audio_render_stream_flush(streams_[i].handle);
    }
  }
}

}  // namespace halpp::audio