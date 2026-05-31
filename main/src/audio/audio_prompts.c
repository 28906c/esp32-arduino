#include "audio/audio_prompts.h"
#include "audio/audio_manager.h"
#include "storage/storage.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "PROMPTS";

/* ---- Prompt ID → TF card path ---- */
static const char *s_prompt_paths[PROMPT_COUNT] = {
    [PROMPT_WORD_APPLE]       = "/sdcard/voca/prompts/word_apple.wav",
    [PROMPT_WORD_CUP]         = "/sdcard/voca/prompts/word_cup.wav",
    [PROMPT_WORD_SUN]         = "/sdcard/voca/prompts/word_sun.wav",
    [PROMPT_WORD_KEY]         = "/sdcard/voca/prompts/word_key.wav",
    [PROMPT_WORD_PHONE]       = "/sdcard/voca/prompts/word_phone.wav",
    [PROMPT_FEEDBACK_CORRECT] = "/sdcard/voca/prompts/feedback_correct.wav",
    [PROMPT_FEEDBACK_WRONG]   = "/sdcard/voca/prompts/feedback_wrong.wav",
    [PROMPT_RECORD_START]     = "/sdcard/voca/prompts/record_start.wav",
    [PROMPT_RECORD_END]       = "/sdcard/voca/prompts/record_end.wav",
};

/* ---- Fallback beep frequencies per prompt type ---- */
static void fallback_beep(prompt_id_t id)
{
    switch (id) {
    case PROMPT_FEEDBACK_CORRECT:
        audio_play_beep(880, 200);
        break;
    case PROMPT_FEEDBACK_WRONG:
        audio_play_beep(220, 400);
        break;
    default:
        audio_play_beep(523, 150);
        break;
    }
}

/* ---- Simple WAV header parser: only PCM, 16kHz, 16-bit, mono ---- */

#pragma pack(push, 1)
typedef struct {
    char     riff[4];       /* "RIFF" */
    uint32_t chunk_size;
    char     wave[4];       /* "WAVE" */
    char     fmt[4];        /* "fmt " */
    uint32_t fmt_size;
    uint16_t audio_format;  /* 1 = PCM */
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_header_t;
#pragma pack(pop)

static bool wav_validate(const wav_header_t *h)
{
    if (memcmp(h->riff, "RIFF", 4) != 0) return false;
    if (memcmp(h->wave, "WAVE", 4) != 0) return false;
    if (memcmp(h->fmt,  "fmt ", 4) != 0) return false;
    if (h->audio_format   != 1)     return false;  /* PCM only */
    if (h->sample_rate    != 16000) return false;
    if (h->bits_per_sample != 16)   return false;
    if (h->num_channels   != 1)     return false;
    return true;
}

/* ---- Public API ---- */

esp_err_t audio_play_prompt(prompt_id_t id)
{
    if (id >= PROMPT_COUNT) {
        ESP_LOGE(TAG, "Invalid prompt id: %d", (int)id);
        return ESP_ERR_INVALID_ARG;
    }

    if (!storage_is_ready()) {
        ESP_LOGW(TAG, "TF card not ready, falling back to beep for id=%d", (int)id);
        fallback_beep(id);
        return ESP_OK;
    }

    const char *path = s_prompt_paths[id];
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "Prompt file not found: %s → fallback beep", path);
        fallback_beep(id);
        return ESP_OK;
    }

    /* Read and validate WAV header */
    wav_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        ESP_LOGE(TAG, "Failed to read WAV header: %s", path);
        fclose(f);
        fallback_beep(id);
        return ESP_OK;
    }

    if (!wav_validate(&header)) {
        ESP_LOGE(TAG, "Unsupported WAV format in %s (fmt=%u, sr=%lu, bits=%u, ch=%u)",
                 path, header.audio_format, (unsigned long)header.sample_rate,
                 header.bits_per_sample, header.num_channels);
        fclose(f);
        fallback_beep(id);
        return ESP_OK;
    }

    /* Skip extra chunks until "data" (max 20 chunks to guard against malformed files) */
    char chunk_id[4];
    uint32_t chunk_size;
    int chunk_iters = 0;
    while (chunk_iters++ < 20) {
        if (fread(chunk_id, 4, 1, f) != 1) break;
        if (fread(&chunk_size, 4, 1, f) != 1) break;
        if (memcmp(chunk_id, "data", 4) == 0) {
            uint32_t samples = chunk_size / 2;
            if (samples > 16000 * 5) { samples = 16000 * 5; }

            int16_t *pcm = malloc(samples * sizeof(int16_t));
            if (!pcm) {
                ESP_LOGE(TAG, "OOM loading prompt %d", (int)id);
                fclose(f);
                fallback_beep(id);
                return ESP_OK;
            }
            size_t read = fread(pcm, sizeof(int16_t), samples, f);
            fclose(f);

            if (read > 0) {
                audio_play_pcm(pcm, (uint32_t)read, 16000);
            } else {
                ESP_LOGE(TAG, "Failed to read PCM data from %s", path);
                fallback_beep(id);
            }
            free(pcm);
            return ESP_OK;
        }
        /* Skip unknown chunk */
        fseek(f, chunk_size, SEEK_CUR);
    }

    ESP_LOGE(TAG, "No data chunk found in %s", path);
    fclose(f);
    fallback_beep(id);
    return ESP_OK;
}
