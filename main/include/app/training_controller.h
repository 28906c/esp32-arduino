#ifndef TRAINING_CONTROLLER_H
#define TRAINING_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "ui/ui_nav.h"

/* Question types */
typedef struct {
    const char *prompt;          /* e.g. "请选出'苹果'" */
    const char *choices[4];      /* 4 option texts */
    uint8_t     correct_index;   /* 0-3 */
} listen_question_t;

typedef struct {
    const char *image_path;      /* M3: NULL, use text placeholder */
    const char *target_word;     /* target word for naming */
    const char *hint;            /* hint text */
} naming_question_t;

#define SENTENCE_MAX_WORDS 6

typedef struct {
    const char *words[SENTENCE_MAX_WORDS];
    uint8_t     word_count;
    uint8_t     correct_order[SENTENCE_MAX_WORDS];
    const char *hint;
} sentence_question_t;

typedef struct {
    uint8_t q_idx;
    uint8_t selected_order[SENTENCE_MAX_WORDS];
    uint8_t selected_count;
} sentence_answer_t;

#define MAX_HISTORY 50

typedef struct {
    screen_id_t module;
    uint8_t     correct;
    uint8_t     total;
    uint32_t    elapsed_s;
    uint32_t    timestamp;
} history_record_t;

/* Session state */
void tc_init(void);
void tc_session_start(screen_id_t module);
void tc_handle_event(uint32_t type, void *payload, int32_t session_id);
void tc_session_abort(void);
void tc_advance_question(void);
bool tc_session_active(void);

/* Question bank access */
const listen_question_t *tc_get_listen_question(uint8_t index);
uint8_t tc_get_listen_count(void);
const naming_question_t *tc_get_naming_question(uint8_t index);
uint8_t tc_get_naming_count(void);
const sentence_question_t *tc_get_sentence_question(uint8_t index);
uint8_t tc_get_sentence_count(void);

/* History */
void               tc_history_add(screen_id_t module, uint8_t correct, uint8_t total, uint32_t elapsed);
void               tc_history_clear(void);
uint8_t            tc_history_count(void);
history_record_t  *tc_history_get(uint8_t index);

#endif
