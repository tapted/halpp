#pragma once

#include <cstddef>
#include <cstdint>

#include "espbase/esp_result.hpp"

typedef void* esp_gmf_element_handle_t;
typedef struct esp_gmf_port_* esp_gmf_port_handle_t;

namespace halpp::audio {

class Microphone;

struct AecProcessorConfig {
  uint8_t filter_len = 4;                 // Recommended tail length for ESP32-S3
  const char* input_format = "MMRN";  // Slot 0=Mic1, Slot 1=Mic2, Slot 2=Ref(Mic3), Slot 3=Unused
  bool high_perf = true;
};

class AecProcessor {
 public:
  explicit constexpr AecProcessor(Microphone& mic) : mic_(mic) {}
  ~AecProcessor();

  EspResult<> begin(const AecProcessorConfig& cfg = {});
  void end();

  bool is_running() const { return is_running_; }

  // Reads clean, echo-cancelled 16-bit mono 16 kHz PCM frames into `mono_samples`.
  // `max_samples` must be at least one AFE frame (typically 256 or 512 samples).
  // Returns the number of mono int16_t samples written.
  EspResult<size_t> read(int16_t* mono_samples, size_t max_samples, uint32_t timeout_ms = 100);

 private:
  class Impl;
  friend class AecProcessor::Impl;
  static constexpr size_t MAX_TDM_FRAME_SAMPLES = 512 * 4;  // 512 frames x 4 TDM slots (4 KB)

  Microphone& mic_;
  bool is_running_ = false;

  esp_gmf_element_handle_t aec_el_ = nullptr;
  esp_gmf_port_handle_t in_port_ = nullptr;
  esp_gmf_port_handle_t out_port_ = nullptr;

  int16_t tdm_scratch_[MAX_TDM_FRAME_SAMPLES]{};

  // Per-call transfer state used by zero-copy port callbacks
  int16_t* out_dst_ = nullptr;
  size_t out_cap_bytes_ = 0;
  size_t out_written_bytes_ = 0;
  uint32_t current_timeout_ms_ = 100;

  AecProcessor(const AecProcessor&) = delete;
  AecProcessor& operator=(const AecProcessor&) = delete;
};

}  // namespace halpp::audio