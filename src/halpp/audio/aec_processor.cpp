#include "halpp/audio/aec_processor.hpp"

#include <cinttypes>
#include <esp_gmf_aec.h>
#include <esp_gmf_element.h>
#include <esp_gmf_obj.h>
#include <esp_gmf_payload.h>
#include <esp_gmf_port.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "halpp/audio/microphone.hpp"

namespace halpp::audio {

namespace {
constexpr const char TAG[] = "GMF_AEC";
}  // namespace

class AecProcessor::Impl {
 public:
  static esp_gmf_err_io_t in_acquire_cb(void* ctx, esp_gmf_payload_t* load, uint32_t wanted_size,
                                        int block_ticks);
  static esp_gmf_err_io_t in_release_cb(void* ctx, esp_gmf_payload_t* load, int block_ticks);
  static esp_gmf_err_io_t out_acquire_cb(void* ctx, esp_gmf_payload_t* load, uint32_t wanted_size,
                                         int block_ticks);
  static esp_gmf_err_io_t out_release_cb(void* ctx, esp_gmf_payload_t* load, int block_ticks);
};

esp_gmf_err_io_t AecProcessor::Impl::in_acquire_cb(void* ctx, esp_gmf_payload_t* load,
                                                   uint32_t wanted_size, int /*block_ticks*/) {
  auto* self = static_cast<AecProcessor*>(ctx);
  if (wanted_size > sizeof(self->tdm_scratch_)) {
    ESP_LOGE(TAG, "Requested TDM chunk (%" PRIu32 " B) exceeds scratch buffer", wanted_size);
    return ESP_GMF_IO_FAIL;
  }

  EspResult<size_t> res =
      self->mic_.read(self->tdm_scratch_, wanted_size, self->current_timeout_ms_);
  if (!res) return ESP_GMF_IO_FAIL;
  if (*res < wanted_size) return ESP_GMF_IO_TIMEOUT;

  load->buf = reinterpret_cast<uint8_t*>(self->tdm_scratch_);
  load->buf_length = sizeof(self->tdm_scratch_);
  load->valid_size = *res;
  load->is_done = false;
  return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t AecProcessor::Impl::in_release_cb(void* /*ctx*/, esp_gmf_payload_t* /*load*/,
                                                   int /*block_ticks*/) {
  return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t AecProcessor::Impl::out_acquire_cb(void* ctx, esp_gmf_payload_t* load,
                                                    uint32_t wanted_size, int /*block_ticks*/) {
  auto* self = static_cast<AecProcessor*>(ctx);
  if (!self->out_dst_ || (self->out_cap_bytes_ - self->out_written_bytes_) < wanted_size) {
    return ESP_GMF_IO_FAIL;
  }

  load->buf = reinterpret_cast<uint8_t*>(self->out_dst_) + self->out_written_bytes_;
  load->buf_length = wanted_size;
  load->valid_size = 0;
  load->is_done = false;
  return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t AecProcessor::Impl::out_release_cb(void* ctx, esp_gmf_payload_t* load,
                                                    int /*block_ticks*/) {
  auto* self = static_cast<AecProcessor*>(ctx);
  self->out_written_bytes_ += load->valid_size;
  return ESP_GMF_IO_OK;
}

AecProcessor::~AecProcessor() {
  end();
}

EspResult<> AecProcessor::begin(const AecProcessorConfig& cfg) {
  if (is_running_) return ESP_OK;

  if (mic_.config().sample_rate != 16000 && mic_.config().sample_rate != 8000) {
    ESP_LOGE(TAG, "ai_aec requires 16 kHz or 8 kHz sample rate (got %" PRIu32 " Hz)",
             mic_.config().sample_rate);
    return ESP_ERR_INVALID_ARG;
  }

  if (EspError err = mic_.start_rx()) {
    return err.log(TAG, "Failed to start microphone RX");
  }

  esp_gmf_aec_cfg_t aec_cfg = {
      .filter_len = cfg.filter_len,
      .type = (mic_.config().sample_rate == 8000) ? AFE_TYPE_VC_8K : AFE_TYPE_VC,
      .mode = cfg.high_perf ? AFE_MODE_HIGH_PERF : AFE_MODE_LOW_COST,
      .input_format = const_cast<char*>(cfg.input_format),
  };

  if (esp_gmf_aec_init(&aec_cfg, &aec_el_) != ESP_GMF_ERR_OK || !aec_el_) {
    mic_.end_rx();
    return EspError(ESP_FAIL).log(TAG, "esp_gmf_aec_init failed");
  }

  esp_gmf_port_config_t in_cfg = {
      .dir = ESP_GMF_PORT_DIR_IN,
      .type = ESP_GMF_PORT_TYPE_BYTE,
      .ops =
          {
              .acquire = &Impl::in_acquire_cb,
              .release = &Impl::in_release_cb,
              .del = nullptr,
          },
      .ctx = this,
      .data_length = 0,
      .wait_ticks = 0,
  };

  esp_gmf_port_config_t out_cfg = {
      .dir = ESP_GMF_PORT_DIR_OUT,
      .type = ESP_GMF_PORT_TYPE_BYTE,
      .ops =
          {
              .acquire = &Impl::out_acquire_cb,
              .release = &Impl::out_release_cb,
              .del = nullptr,
          },
      .ctx = this,
      .data_length = 0,
      .wait_ticks = 0,
  };

  if (esp_gmf_port_init(&in_cfg, &in_port_) != ESP_GMF_ERR_OK ||
      esp_gmf_port_init(&out_cfg, &out_port_) != ESP_GMF_ERR_OK) {
    if (in_port_) {
      esp_gmf_port_deinit(in_port_);
      in_port_ = nullptr;
    }
    if (out_port_) {
      esp_gmf_port_deinit(out_port_);
      out_port_ = nullptr;
    }
    esp_gmf_obj_delete(aec_el_);
    aec_el_ = nullptr;
    mic_.end_rx();
    return EspError(ESP_FAIL).log(TAG, "Failed to initialize GMF ports");
  }

  // Ownership of in_port_ and out_port_ transfers to aec_el_ upon registration
  esp_gmf_element_register_in_port(aec_el_, in_port_);
  esp_gmf_element_register_out_port(aec_el_, out_port_);
  in_port_ = nullptr;
  out_port_ = nullptr;

  if (esp_gmf_element_process_open(aec_el_, nullptr) != ESP_GMF_JOB_ERR_OK) {
    esp_gmf_obj_delete(aec_el_);
    aec_el_ = nullptr;
    mic_.end_rx();
    return EspError(ESP_FAIL).log(TAG, "esp_gmf_element_process_open failed");
  }

  is_running_ = true;
  ESP_LOGI(TAG, "GMF ai_aec started (format=%s, rate=%" PRIu32 " Hz)", cfg.input_format,
           mic_.config().sample_rate);
  return ESP_OK;
}

void AecProcessor::end() {
  if (!aec_el_ && !is_running_) return;

  if (aec_el_) {
    if (is_running_) {
      esp_gmf_element_process_close(aec_el_, nullptr);
    }
    // Automatically calls esp_gmf_port_deinit() on registered in/out ports
    esp_gmf_obj_delete(aec_el_);
    aec_el_ = nullptr;
  }
  in_port_ = nullptr;
  out_port_ = nullptr;

  if (is_running_) {
    is_running_ = false;
    mic_.end_rx();
  }
}

EspResult<size_t> AecProcessor::read(int16_t* mono_samples, size_t max_samples,
                                     uint32_t timeout_ms) {
  if (!is_running_ || !mono_samples || max_samples == 0) {
    return EspError(ESP_ERR_INVALID_STATE);
  }

  out_dst_ = mono_samples;
  out_cap_bytes_ = max_samples * sizeof(int16_t);
  out_written_bytes_ = 0;

  const TickType_t start_ticks = xTaskGetTickCount();
  const TickType_t budget_ticks = pdMS_TO_TICKS(timeout_ms);

  esp_gmf_job_err_t job_err = ESP_GMF_JOB_ERR_OK;
  while (out_written_bytes_ == 0) {
    const TickType_t elapsed = xTaskGetTickCount() - start_ticks;
    if (elapsed >= budget_ticks && timeout_ms > 0) {
      current_timeout_ms_ = 0;
    } else {
      current_timeout_ms_ = (timeout_ms == 0) ? 0 : pdTICKS_TO_MS(budget_ticks - elapsed);
    }

    job_err = esp_gmf_element_process_running(aec_el_, nullptr);
    if (job_err == ESP_GMF_JOB_ERR_OK) {
      break;
    }
    if (job_err != ESP_GMF_JOB_ERR_CONTINUE) {
      out_dst_ = nullptr;
      return EspError(ESP_FAIL);
    }
    if (timeout_ms == 0 || (xTaskGetTickCount() - start_ticks) >= budget_ticks) {
      break;
    }
  }

  out_dst_ = nullptr;
  return out_written_bytes_ / sizeof(int16_t);
}

}  // namespace halpp::audio