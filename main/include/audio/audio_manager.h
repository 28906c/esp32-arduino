#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Init / beep / volume
 * ================================================================ */

esp_err_t audio_init(void);
esp_err_t audio_play_beep(int freq_hz, int duration_ms);
esp_err_t audio_play_pcm(const int16_t *samples, uint32_t sample_count, uint32_t sample_rate);
esp_err_t audio_set_volume(int vol);
int       audio_get_volume(void);

/* ================================================================
 *  M4.2: PCM recording
 * ================================================================ */

typedef enum {
    AUDIO_REC_STOP_USER,        /* user released button */
    AUDIO_REC_STOP_TIMEOUT,     /* 5 s auto-stop */
    AUDIO_REC_STOP_BUFFER_FULL, /* PSRAM buffer exhausted */
    AUDIO_REC_STOP_ERROR,       /* I2S or HW error */
} audio_rec_stop_reason_t;

typedef struct {
    int16_t                 *buffer;       /* PSRAM PCM samples, 16-bit mono */
    uint32_t                 sample_count; /* number of int16_t samples */
    uint32_t                 sample_rate;  /* 16000 */
    uint8_t                  bits;         /* 16 */
    uint8_t                  channels;     /* 1 */
    audio_rec_stop_reason_t  stop_reason;
} audio_recording_t;

/* Start capturing from I2S RX. Allocates PSRAM buffer (max 160 KB).
 * Fails if already recording. */
esp_err_t audio_rec_start(void);

/* Stop capturing. Transfers buffer ownership to *out_rec.
 * Caller MUST call audio_recording_free() when done. */
esp_err_t audio_rec_stop(audio_recording_t **out_rec);

/* Release recording buffer + struct. Safe to call with NULL. */
void audio_recording_free(audio_recording_t *rec);

/* Returns true if currently recording. */
bool audio_rec_is_active(void);

/* Read one chunk from I2S RX (called by audio task in recording loop).
 * Returns bytes read, 0 on error/buffer-full. */
uint32_t audio_rec_read_chunk(void);

/* Set stop reason for the current recording. */
void audio_rec_set_stop_reason(audio_rec_stop_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_MANAGER_H */
