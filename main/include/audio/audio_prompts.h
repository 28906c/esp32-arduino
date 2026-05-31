#ifndef AUDIO_PROMPTS_H
#define AUDIO_PROMPTS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROMPT_WORD_APPLE,         /* /sdcard/voca/prompts/word_apple.wav    */
    PROMPT_WORD_CUP,           /* /sdcard/voca/prompts/word_cup.wav      */
    PROMPT_WORD_SUN,           /* /sdcard/voca/prompts/word_sun.wav      */
    PROMPT_WORD_KEY,           /* /sdcard/voca/prompts/word_key.wav      */
    PROMPT_WORD_PHONE,         /* /sdcard/voca/prompts/word_phone.wav    */
    PROMPT_FEEDBACK_CORRECT,   /* /sdcard/voca/prompts/feedback_correct.wav */
    PROMPT_FEEDBACK_WRONG,     /* /sdcard/voca/prompts/feedback_wrong.wav   */
    PROMPT_RECORD_START,       /* /sdcard/voca/prompts/record_start.wav     */
    PROMPT_RECORD_END,         /* /sdcard/voca/prompts/record_end.wav       */
    PROMPT_COUNT
} prompt_id_t;

/* Play a named prompt from TF card. Falls back to sine beep if:
 *   - TF card not mounted
 *   - WAV file not found
 *   - WAV format is not 16kHz / 16-bit / mono / PCM
 *
 * Only call from audio task context (codec ownership).
 */
esp_err_t audio_play_prompt(prompt_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROMPTS_H */
