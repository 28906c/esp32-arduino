#include "audio/audio_manager.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "bsp/esp32_p4_nano.h"
#include "driver/i2s_std.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "AUDIO_MGR";

/* ---- Custom GPIO pin mapping (ESP32-P4-Module-DEV-KIT 40Pin → ES8311 module) ---- */
#define AUDIO_I2S_MCLK  GPIO_NUM_48
#define AUDIO_I2S_BCLK  GPIO_NUM_47
#define AUDIO_I2S_LRCK  GPIO_NUM_45
#define AUDIO_I2S_DOUT  GPIO_NUM_54  /* ESP32 → module DI  */
#define AUDIO_I2S_DIN   GPIO_NUM_46  /* module DO → ESP32 */

/* No separate PA_CTRL pin on this module; NS4150B is always-on with power */
#define AUDIO_PA_PIN    GPIO_NUM_NC

/* ---- Internal state ---- */
static esp_codec_dev_handle_t s_speaker = NULL;
static i2s_chan_handle_t      s_i2s_tx  = NULL;
static i2s_chan_handle_t      s_i2s_rx  = NULL;
static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static int  s_volume  = 80;
static bool s_inited  = false;

#define ES8311_GP_REG45  0x45
static void es8311_gpio_set_amp(bool enable)
{
    if (!s_ctrl_if) return;
    uint8_t val = enable ? 0xC0 : 0x00;
    ESP_LOGI(TAG, "ES8311 GPIO reg 0x45 ← 0x%02X (%s)", val, enable ? "PA ON" : "PA OFF");
    s_ctrl_if->write_reg(s_ctrl_if, ES8311_GP_REG45, 1, &val, 1);
}

/* ---- Forward declarations ---- */
static esp_err_t i2s_channel_setup(void);

esp_err_t audio_init(void)
{
    if (s_inited) return ESP_OK;

    /* 1. Create I2S duplex channels on I2S_NUM_0 with custom GPIOs */
    esp_err_t ret = i2s_channel_setup();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel setup failed: %d", ret);
        return ret;
    }

    /* 2. Wrap I2S channels as codec data interface */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = 0,  /* I2S_NUM_0 */
        .tx_handle = s_i2s_tx,
        .rx_handle = s_i2s_rx,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (!data_if) {
        ESP_LOGE(TAG, "Failed to create I2S data interface");
        return ESP_FAIL;
    }

    /* 3. Get shared I2C bus handle (already initialized by BSP for touch) */
    void *i2c_handle = bsp_i2c_get_handle();
    if (!i2c_handle) {
        ESP_LOGE(TAG, "I2C bus not ready (touch may not be initialized yet)");
        return ESP_ERR_INVALID_STATE;
    }

    audio_codec_i2c_cfg_t i2c_ctrl_cfg = {
        .port       = BSP_I2C_NUM,
        .addr       = ES8311_CODEC_DEFAULT_ADDR,  /* 0x30 */
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
    if (!ctrl_if) {
        ESP_LOGE(TAG, "Failed to create I2C control interface");
        return ESP_FAIL;
    }
    s_ctrl_if = ctrl_if;

    /* 4. Create GPIO interface (no PA pin, no reset pin) */
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    /* 5. Create ES8311 codec instance for speaker (DAC only) */
    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage       = 3.3f,
        .codec_dac_voltage = 3.3f,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if     = ctrl_if,
        .gpio_if     = gpio_if,
        .codec_mode  = ESP_CODEC_DEV_TYPE_OUT,      /* DAC output */
        .pa_pin      = AUDIO_PA_PIN,                 /* NS4150B always-on */
        .pa_reverted = false,
        .master_mode = false,                        /* ESP32 is I2S master */
        .use_mclk    = true,                         /* MCLK from ESP32 */
        .digital_mic = false,                        /* Analog mic */
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain     = gain,
        .mclk_div    = 256,
    };

    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);
    if (!codec_if) {
        ESP_LOGE(TAG, "Failed to create ES8311 codec interface");
        return ESP_FAIL;
    }

    /* 6. Create codec device */
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,       /* Share for future mic use */
        .codec_if = codec_if,
        .data_if  = data_if,
    };
    s_speaker = esp_codec_dev_new(&dev_cfg);
    if (!s_speaker) {
        ESP_LOGE(TAG, "Failed to create codec device");
        return ESP_FAIL;
    }

    /* 7. Apply default volume and open codec (kept open for low-latency beeps) */
    esp_codec_dev_set_out_vol(s_speaker, s_volume);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel         = 1,
        .channel_mask    = 0,
        .sample_rate     = 16000,
        .mclk_multiple   = I2S_MCLK_MULTIPLE_256,
    };
    ret = esp_codec_dev_open(s_speaker, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "codec open failed: %d", ret);
        return ret;
    }

    s_inited = true;
    ESP_LOGI(TAG, "Audio ready (ES8311+NS4150B, I2S=0, GPIO 45-48,54, I2C=7/8, vol=%d)", s_volume);
    return ESP_OK;
}

/* -----------------------------------------------------------------------------
 *  Sine-wave beep — codec kept open since audio_init() for low latency.
 * ---------------------------------------------------------------------------*/
esp_err_t audio_play_beep(int freq_hz, int duration_ms)
{
    if (!s_speaker) return ESP_ERR_INVALID_STATE;

    const int sample_rate = 16000;
    const int sample_count = sample_rate * duration_ms / 1000;
    if (sample_count < 1) return ESP_ERR_INVALID_ARG;

    const size_t buf_bytes = sample_count * sizeof(int16_t);
    int16_t *buf = malloc(buf_bytes);
    if (!buf) return ESP_ERR_NO_MEM;

    const double amplitude = 12000.0;
    for (int i = 0; i < sample_count; i++) {
        buf[i] = (int16_t)(amplitude * sin(2.0 * M_PI * freq_hz * i / sample_rate));
    }

    es8311_gpio_set_amp(true);
    int ret = esp_codec_dev_write(s_speaker, buf, (int)buf_bytes);
    es8311_gpio_set_amp(false);

    if (ret < 0) ESP_LOGE(TAG, "beep %dHz/%dms write failed: %d", freq_hz, duration_ms, ret);
    free(buf);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t audio_set_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    s_volume = vol;
    if (s_speaker) esp_codec_dev_set_out_vol(s_speaker, vol);
    return ESP_OK;
}

int audio_get_volume(void) { return s_volume; }

/*
 * Play raw PCM samples through I2S TX. Used for replaying recordings.
 */
esp_err_t audio_play_pcm(const int16_t *samples, uint32_t sample_count, uint32_t sample_rate)
{
    if (!s_speaker || !samples || sample_count == 0) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "play_pcm: %lu samples @ %lu Hz", (unsigned long)sample_count,
             (unsigned long)sample_rate);
    es8311_gpio_set_amp(true);
    int ret = esp_codec_dev_write(s_speaker, (void *)samples, (int)(sample_count * sizeof(int16_t)));
    es8311_gpio_set_amp(false);

    if (ret < 0) {
        ESP_LOGE(TAG, "play_pcm write failed: %d", ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ================================================================
 *  M4.2: PCM Recording
 * ================================================================ */

#define REC_SAMPLE_RATE  16000
#define REC_MAX_SECONDS  5
#define REC_MAX_SAMPLES  (REC_SAMPLE_RATE * REC_MAX_SECONDS)   /* 80000 */
#define REC_MAX_BYTES    (REC_MAX_SAMPLES * sizeof(int16_t))   /* 160000 */
#define REC_CHUNK_SAMPLES 512

static struct {
    int16_t                 *buffer;
    uint32_t                 bytes_used;
    bool                     active;
    audio_rec_stop_reason_t  stop_reason;
} s_rec;

esp_err_t audio_rec_start(void)
{
    if (s_rec.active) {
        ESP_LOGE(TAG, "rec_start: already recording");
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_i2s_rx) {
        ESP_LOGE(TAG, "rec_start: no I2S RX channel");
        return ESP_ERR_INVALID_STATE;
    }

    s_rec.buffer = heap_caps_malloc(REC_MAX_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_rec.buffer) {
        ESP_LOGE(TAG, "rec_start: PSRAM alloc failed (%lu bytes)",
                 (unsigned long)REC_MAX_BYTES);
        s_rec.active     = false;
        s_rec.stop_reason = AUDIO_REC_STOP_ERROR;
        return ESP_ERR_NO_MEM;
    }

    s_rec.bytes_used  = 0;
    s_rec.active      = true;
    s_rec.stop_reason = AUDIO_REC_STOP_USER;

    /* Note: i2s_channel_flush_rx() not available in all IDF 5.x versions.
     * Stale RX samples at recording start are harmless (< 1 chunk). */
    ESP_LOGI(TAG, "Recording started (max %lu KB in PSRAM)",
             (unsigned long)(REC_MAX_BYTES / 1024));
    return ESP_OK;
}

esp_err_t audio_rec_stop(audio_recording_t **out_rec)
{
    if (!s_rec.active) {
        ESP_LOGE(TAG, "rec_stop: not recording");
        return ESP_ERR_INVALID_STATE;
    }

    s_rec.active = false;

    audio_recording_t *rec = malloc(sizeof(audio_recording_t));
    if (!rec) {
        /* Desperate — free buffer, can't return recording */
        heap_caps_free(s_rec.buffer);
        s_rec.buffer = NULL;
        *out_rec = NULL;
        return ESP_ERR_NO_MEM;
    }

    rec->buffer       = s_rec.buffer;
    rec->sample_count = s_rec.bytes_used / sizeof(int16_t);
    rec->sample_rate  = REC_SAMPLE_RATE;
    rec->bits         = 16;
    rec->channels     = 1;
    rec->stop_reason  = s_rec.stop_reason;

    s_rec.buffer = NULL;

    ESP_LOGI(TAG, "Recording stopped: %lu samples, reason=%d",
             (unsigned long)rec->sample_count, (int)rec->stop_reason);

    *out_rec = rec;
    return ESP_OK;
}

void audio_recording_free(audio_recording_t *rec)
{
    if (!rec) return;
    if (rec->buffer) {
        heap_caps_free(rec->buffer);
    }
    free(rec);
}

bool audio_rec_is_active(void)
{
    return s_rec.active;
}

/*
 * Read one chunk from I2S RX into the recording buffer.
 * Returns number of bytes read, 0 on error/timeout.
 * Call only from audio task context.
 */
uint32_t audio_rec_read_chunk(void)
{
    if (!s_rec.active || !s_rec.buffer) return 0;

    uint32_t remaining = REC_MAX_BYTES - s_rec.bytes_used;
    if (remaining < REC_CHUNK_SAMPLES * sizeof(int16_t)) {
        s_rec.stop_reason = AUDIO_REC_STOP_BUFFER_FULL;
        s_rec.active = false;
        ESP_LOGW(TAG, "Recording buffer full");
        return 0;
    }

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(s_i2s_rx,
                                     s_rec.buffer + s_rec.bytes_used / sizeof(int16_t),
                                     REC_CHUNK_SAMPLES * sizeof(int16_t),
                                     &bytes_read,
                                     pdMS_TO_TICKS(100));
    if (ret != ESP_OK && bytes_read == 0) {
        ESP_LOGE(TAG, "I2S RX read error: %d", (int)ret);
        return 0;
    }

    s_rec.bytes_used += bytes_read;
    return bytes_read;
}

void audio_rec_set_stop_reason(audio_rec_stop_reason_t reason)
{
    s_rec.stop_reason = reason;
}

/* -----------------------------------------------------------------------------
 *  Internal: create duplex I2S channels with custom GPIO pins
 * ---------------------------------------------------------------------------*/
static esp_err_t i2s_channel_setup(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_i2s_tx, &s_i2s_rx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %d", ret);
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_MCLK,
            .bclk = AUDIO_I2S_BCLK,
            .ws   = AUDIO_I2S_LRCK,
            .dout = AUDIO_I2S_DOUT,
            .din  = AUDIO_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    if (s_i2s_tx) {
        ret = i2s_channel_init_std_mode(s_i2s_tx, &std_cfg);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "tx init failed: %d", ret); return ret; }
        ret = i2s_channel_enable(s_i2s_tx);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "tx enable failed: %d", ret); return ret; }
    }
    if (s_i2s_rx) {
        ret = i2s_channel_init_std_mode(s_i2s_rx, &std_cfg);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "rx init failed: %d", ret); return ret; }
        ret = i2s_channel_enable(s_i2s_rx);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "rx enable failed: %d", ret); return ret; }
    }

    return ESP_OK;
}
