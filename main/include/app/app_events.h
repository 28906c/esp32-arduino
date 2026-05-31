#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "training_controller.h"

/* ================================================================
 * 跨任务 typed message — 所有模块间通信统一使用 app_msg_t
 * 发送方 allocate payload，接收方 free payload
 * ================================================================ */

/* ---- UI -> App Controller ---- */
typedef enum {
    UI_EVT_BOOT_READY            = 0x1000,
    UI_EVT_PATIENT_SELECTED      = 0x1001,
    UI_EVT_PATIENT_CREATE_REQUESTED = 0x1002,
    UI_EVT_MODULE_SELECTED       = 0x1003,
    UI_EVT_ANSWER_SELECTED       = 0x1004,
    UI_EVT_SENTENCE_WORD_SELECTED = 0x1005,
    UI_EVT_SENTENCE_CONFIRM      = 0x1006,
    UI_EVT_RECORD_PRESSED        = 0x1007,
    UI_EVT_REPLAY_PRESSED        = 0x1008,
    UI_EVT_BACK_PRESSED          = 0x1009,
    UI_EVT_SETTINGS_CHANGED      = 0x100A,
    UI_EVT_FEEDBACK_DONE         = 0x100B,
} ui_event_type_t;

/* ---- App Controller -> UI ---- */
typedef enum {
    UI_CMD_SHOW_SCREEN           = 0x2000,
    UI_CMD_RENDER_HOME           = 0x2001,
    UI_CMD_RENDER_QUESTION       = 0x2002,
    UI_CMD_SHOW_FEEDBACK         = 0x2003,
    UI_CMD_SHOW_BUSY             = 0x2004,
    UI_CMD_SHOW_RESULT           = 0x2005,
    UI_CMD_SHOW_ERROR            = 0x2006,
    UI_CMD_UPDATE_STATUS         = 0x2007,
    UI_CMD_RECORDING_READY       = 0x2008,
} ui_cmd_type_t;

/* ---- App Controller -> Audio ---- */
typedef enum {
    AUDIO_CMD_PLAY_PROMPT        = 0x3000,
    AUDIO_CMD_PLAY_FEEDBACK      = 0x3001,
    AUDIO_CMD_START_RECORDING    = 0x3002,
    AUDIO_CMD_STOP_RECORDING     = 0x3003,
    AUDIO_CMD_SET_VOLUME         = 0x3004,
    AUDIO_CMD_REPLAY_RECORDING   = 0x3005,
} audio_cmd_type_t;

/* ---- Audio/AI/Storage -> App Controller ---- */
typedef enum {
    APP_EVT_RECORDING_STARTED    = 0x4000,
    APP_EVT_RECORDING_DONE       = 0x4001,
    APP_EVT_RECOGNITION_DONE     = 0x4002,
    APP_EVT_SCORE_DONE           = 0x4003,
    APP_EVT_AUDIO_ERROR          = 0x4004,
    APP_EVT_STORAGE_DONE         = 0x4005,
    APP_EVT_STORAGE_ERROR        = 0x4006,
} app_event_type_t;

/* ---- Diagnostic ---- */
typedef enum {
    DIAG_CMD_HEAP                = 0x5000,
    DIAG_CMD_TASKS               = 0x5001,
    DIAG_CMD_STATUS              = 0x5002,
    DIAG_CMD_LOG_LEVEL           = 0x5003,
    DIAG_CMD_REBOOT              = 0x5004,
} diag_cmd_type_t;

/* ---- M3 Payload Structs ---- */

typedef struct {
    int32_t module;     /* screen_id_t of selected training module */
} payload_module_selected_t;

typedef struct {
    uint8_t question_idx;
    uint8_t answer_idx;
} payload_answer_t;

typedef struct {
    uint8_t q_idx;
    uint8_t total;
    /* question data follows in-line (cast to listen_question_t*) */
} payload_render_q_t;

typedef struct {
    bool     correct;
    uint8_t  answer_idx;       /* user's choice */
    uint8_t  correct_idx;      /* correct answer */
} payload_feedback_t;

typedef struct {
    uint8_t  correct_cnt;
    uint8_t  total;
    uint32_t elapsed_s;
} payload_result_t;

typedef struct {
    bool pressed;   /* true = press, false = release */
} payload_record_t;

typedef struct {
    int volume;     /* 0-100 */
} payload_volume_t;

typedef struct {
    uint8_t q_idx;
    uint8_t selected_order[SENTENCE_MAX_WORDS];  /* from training_controller.h */
    uint8_t selected_count;
} payload_sentence_answer_t;

/* ---- 统一消息结构 ---- */
typedef struct {
    uint32_t    type;           /* enum value from above */
    int32_t     session_id;     /* 防跨轮次污染 */
    uint32_t    payload_len;    /* payload buffer size, 0 if none */
    void       *payload;        /* owned by sender, freed by receiver */
    int32_t     error_code;     /* 0 = ok, negative = ESP_ERR_xxx */
} app_msg_t;

/* ---- Send helper: free payload on queue-full to prevent leak ---- */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static inline void queue_send_or_free(QueueHandle_t q, app_msg_t *msg)
{
    if (xQueueSend(q, msg, 0) != pdTRUE) {
        free(msg->payload);
    }
}

#endif /* APP_EVENTS_H */
