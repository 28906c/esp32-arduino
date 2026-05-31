#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "app/training_controller.h"
#include "app/app_events.h"
#include "app/app_tasks.h"

static const char *TAG = "TC";

/* ================================================================
 *  Demo Question Bank (M3 hard-coded, M7 replaced by TF storage)
 * ================================================================ */

static const listen_question_t s_listen_bank[] = {
    {"请选出'苹果'",
     {"香蕉", "苹果", "橘子", "西瓜"}, 1},
    {"请选出'喝水'相关",
     {"杯子", "跑步", "睡觉", "看书"}, 0},
    {"请选出正确的颜色名",
     {"红色", "大声", "吃饭", "走路"}, 0},
    {"请选出'太阳'相关的",
     {"下雨", "阳光", "下雪", "刮风"}, 1},
    {"请选出正确的数字",
     {"三", "快", "高", "慢"}, 0},
};

static const naming_question_t s_naming_bank[] = {
    {NULL, "苹果",   "一种红色的水果，很甜"},
    {NULL, "杯子",   "用来喝水的容器"},
    {NULL, "太阳",   "白天挂在天上的"},
    {NULL, "钥匙",   "用来开门的"},
    {NULL, "电话",   "用来和远处的人说话"},
};

static const sentence_question_t s_sentence_bank[] = {
    {{"学校", "去", "我"},             3, {2, 1, 0},       "一个表达去处的句子"},
    {{"看书", "我", "喜欢"},            3, {1, 2, 0},       "一个关于爱好的句子"},
    {{"在",   "做饭", "妈妈"},          3, {2, 0, 1},       "妈妈在厨房干什么？"},
    {{"游戏", "一起", "玩", "我们"},     4, {3, 1, 2, 0},    "小朋友们在一起"},
    {{"公园", "去", "跑步", "早上", "我"}, 5, {3, 4, 1, 0, 2}, "描述早晨的活动"},
};

/* ================================================================
 *  Session State Machine
 * ================================================================ */

static int32_t s_next_session_id = 1;  /* 0 = never valid, monotonic */

static struct {
    int32_t     session_id;
    screen_id_t module;
    uint8_t     current_q;
    uint8_t     correct_cnt;
    TickType_t  start_time;
    bool        active;
} s_sess;

/* History (TODO: M7 migrate to storage module) */
static history_record_t s_history[MAX_HISTORY];
static uint8_t          s_history_count;

void tc_init(void)
{
    memset(&s_sess, 0, sizeof(s_sess));
}

static void tc_send_render_q(uint8_t total)
{
    payload_render_q_t *p = malloc(sizeof(payload_render_q_t));
    if (!p) { ESP_LOGE(TAG, "OOM"); return; }
    p->q_idx = 0;
    p->total = total;

    app_msg_t msg = {
        .type = UI_CMD_RENDER_QUESTION,
        .session_id = s_sess.session_id,
        .payload_len = sizeof(payload_render_q_t),
        .payload = p,
        .error_code = 0,
    };
    queue_send_or_free(g_ui_queue, &msg);
}

void tc_session_start(screen_id_t module)
{
    s_sess.session_id  = s_next_session_id++;
    s_sess.module      = module;
    s_sess.current_q   = 0;
    s_sess.correct_cnt = 0;
    s_sess.start_time  = xTaskGetTickCount();
    s_sess.active      = true;

    printf("[TC   ] Session start: module=%d\n", (int)module);

    if (module == SCREEN_LISTEN_TRAINING && tc_get_listen_count() > 0) {
        tc_send_render_q(tc_get_listen_count());
    } else if (module == SCREEN_NAMING_TRAINING && tc_get_naming_count() > 0) {
        tc_send_render_q(tc_get_naming_count());
    } else if (module == SCREEN_SENTENCE_BUILDING && tc_get_sentence_count() > 0) {
        tc_send_render_q(tc_get_sentence_count());
    } else {
        /* Unimplemented module — show placeholder screen, no real session */
        s_sess.active = false;
        payload_render_q_t *p = malloc(sizeof(payload_render_q_t));
        if (!p) { ESP_LOGE(TAG, "OOM"); return; }
        p->q_idx = 0;
        p->total = 0;
        app_msg_t msg = {
            .type = UI_CMD_RENDER_QUESTION,
            .session_id = s_sess.session_id,
            .payload_len = sizeof(payload_render_q_t),
            .payload = p,
            .error_code = 0,
        };
        queue_send_or_free(g_ui_queue, &msg);
    }
}

static void tc_evaluate_answer(uint8_t answer_idx)
{
    if (!s_sess.active) return;

    const listen_question_t *q = tc_get_listen_question(s_sess.current_q);
    if (!q) return;

    bool correct = (answer_idx == q->correct_index);
    if (correct) s_sess.correct_cnt++;

    printf("[TC   ] Q%d: user=%d correct=%d → %s\n",
           s_sess.current_q, answer_idx, q->correct_index,
           correct ? "OK" : "WRONG");

    /* Send visual feedback to UI */
    payload_feedback_t *p_ui = malloc(sizeof(payload_feedback_t));
    if (!p_ui) { ESP_LOGE(TAG, "OOM p_ui"); return; }
    p_ui->correct     = correct;
    p_ui->answer_idx  = answer_idx;
    p_ui->correct_idx = q->correct_index;

    app_msg_t ui_msg = {
        .type = UI_CMD_SHOW_FEEDBACK,
        .session_id = s_sess.session_id,
        .payload_len = sizeof(payload_feedback_t),
        .payload = p_ui,
        .error_code = 0,
    };
    queue_send_or_free(g_ui_queue, &ui_msg);

    /* Send audio feedback (beep) to audio task */
    payload_feedback_t *p_audio = malloc(sizeof(payload_feedback_t));
    if (!p_audio) { ESP_LOGE(TAG, "OOM p_audio"); return; }
    p_audio->correct     = correct;
    p_audio->answer_idx  = answer_idx;
    p_audio->correct_idx = q->correct_index;

    app_msg_t audio_msg = {
        .type = AUDIO_CMD_PLAY_FEEDBACK,
        .session_id = s_sess.session_id,
        .payload_len = sizeof(payload_feedback_t),
        .payload = p_audio,
        .error_code = 0,
    };
    queue_send_or_free(g_audio_queue, &audio_msg);
}

static void tc_evaluate_sentence(payload_sentence_answer_t *ans)
{
    if (!s_sess.active) return;

    const sentence_question_t *q = tc_get_sentence_question(s_sess.current_q);
    if (!q) return;

    bool correct = false;
    if (ans->selected_count == q->word_count) {
        correct = true;
        for (uint8_t i = 0; i < q->word_count; i++) {
            if (ans->selected_order[i] != q->correct_order[i]) {
                correct = false;
                break;
            }
        }
    }
    if (correct) s_sess.correct_cnt++;

    printf("[TC   ] Sentence Q%d: correct=%d → %s\n",
           s_sess.current_q, q->word_count, correct ? "OK" : "WRONG");

    payload_feedback_t *p_ui = malloc(sizeof(payload_feedback_t));
    if (!p_ui) { ESP_LOGE(TAG, "OOM p_ui"); return; }
    p_ui->correct     = correct;
    p_ui->answer_idx  = 0;
    p_ui->correct_idx = 0;

    app_msg_t ui_msg = {
        .type = UI_CMD_SHOW_FEEDBACK,
        .session_id = s_sess.session_id,
        .payload_len = sizeof(payload_feedback_t),
        .payload = p_ui,
        .error_code = 0,
    };
    queue_send_or_free(g_ui_queue, &ui_msg);

    payload_feedback_t *p_audio = malloc(sizeof(payload_feedback_t));
    if (!p_audio) { ESP_LOGE(TAG, "OOM p_audio"); return; }
    p_audio->correct     = correct;
    p_audio->answer_idx  = 0;
    p_audio->correct_idx = 0;

    app_msg_t audio_msg = {
        .type = AUDIO_CMD_PLAY_FEEDBACK,
        .session_id = s_sess.session_id,
        .payload_len = sizeof(payload_feedback_t),
        .payload = p_audio,
        .error_code = 0,
    };
    queue_send_or_free(g_audio_queue, &audio_msg);
}

static void tc_next_question(void)
{
    if (!s_sess.active) return;

    s_sess.current_q++;
    uint8_t total;
    switch (s_sess.module) {
        case SCREEN_LISTEN_TRAINING:   total = tc_get_listen_count();   break;
        case SCREEN_NAMING_TRAINING:   total = tc_get_naming_count();   break;
        case SCREEN_SENTENCE_BUILDING: total = tc_get_sentence_count(); break;
        default:                       total = 0;                       break;
    }

    if (s_sess.current_q >= total) {
        /* Session complete */
        uint32_t elapsed = (xTaskGetTickCount() - s_sess.start_time)
                         * portTICK_PERIOD_MS / 1000;

        /* M3: naming training has no ASR scoring — pass-through all correct */
        if (s_sess.module == SCREEN_NAMING_TRAINING) {
            s_sess.correct_cnt = total;
        }

        printf("[TC   ] Session done: %d/%d correct, %lu s\n",
               s_sess.correct_cnt, total, (unsigned long)elapsed);

        tc_history_add(s_sess.module, s_sess.correct_cnt, total, elapsed);

        payload_result_t *p = malloc(sizeof(payload_result_t));
        if (!p) { ESP_LOGE(TAG, "OOM result"); return; }
        p->correct_cnt = s_sess.correct_cnt;
        p->total       = total;
        p->elapsed_s   = elapsed;

        app_msg_t msg = {
            .type = UI_CMD_SHOW_RESULT,
            .session_id = s_sess.session_id,
            .payload_len = sizeof(payload_result_t),
            .payload = p,
            .error_code = 0,
        };
        queue_send_or_free(g_ui_queue, &msg);
        s_sess.active = false;
    } else {
        /* Next question */
        payload_render_q_t *p = malloc(sizeof(payload_render_q_t));
        if (!p) { ESP_LOGE(TAG, "OOM next_q"); return; }
        p->q_idx = s_sess.current_q;
        p->total = total;

        app_msg_t msg = {
            .type = UI_CMD_RENDER_QUESTION,
            .session_id = s_sess.session_id,
            .payload_len = sizeof(payload_render_q_t),
            .payload = p,
            .error_code = 0,
        };
        queue_send_or_free(g_ui_queue, &msg);
    }
}

/*
 * Event dispatch — PAYLOAD OWNERSHIP RULE:
 *   Callee reads payload only, does NOT free it.
 *   Caller (app_task_main) always frees msg.payload after return.
 *   Adding a free() inside any branch below WILL cause double-free.
 */
void tc_handle_event(uint32_t type, void *payload, int32_t session_id)
{
    if (!s_sess.active) return;
    /* session_id=0 → UI-originated, always for current session.
     * Non-zero mismatch → stale message from previous session. */
    if (session_id != 0 && session_id != s_sess.session_id) return;

    switch (type) {
    case UI_EVT_ANSWER_SELECTED: {
        if (s_sess.module != SCREEN_LISTEN_TRAINING) break;
        payload_answer_t *p = (payload_answer_t *)payload;
        tc_evaluate_answer(p->answer_idx);
        break;
    }
    case UI_EVT_RECORD_PRESSED: {
        /* Naming training recording — forward to audio task */
        payload_record_t *p = (payload_record_t *)payload;
        app_msg_t msg = {
            .type = p->pressed ? AUDIO_CMD_START_RECORDING
                               : AUDIO_CMD_STOP_RECORDING,
            .session_id = s_sess.session_id,
            .payload_len = 0,
            .payload = NULL,
            .error_code = 0,
        };
        queue_send_or_free(g_audio_queue, &msg);
        printf("[TC   ] Recording %s\n", p->pressed ? "START" : "STOP");
        break;
    }
    case UI_EVT_REPLAY_PRESSED: {
        app_msg_t msg = {
            .type = AUDIO_CMD_PLAY_PROMPT,
            .session_id = s_sess.session_id,
            .payload_len = 0,
            .payload = NULL,
            .error_code = 0,
        };
        queue_send_or_free(g_audio_queue, &msg);
        break;
    }
    case UI_EVT_SENTENCE_CONFIRM: {
        if (s_sess.module != SCREEN_SENTENCE_BUILDING) break;
        payload_sentence_answer_t *ans = (payload_sentence_answer_t *)payload;
        tc_evaluate_sentence(ans);
        break;
    }
    case UI_EVT_FEEDBACK_DONE:
        tc_next_question();
        break;
    default:
        break;
    }
}

void tc_session_abort(void)
{
    printf("[TC   ] Session aborted\n");
    s_sess.active = false;
}

bool tc_session_active(void)
{
    return s_sess.active;
}

/* Question bank accessors */

const listen_question_t *tc_get_listen_question(uint8_t index)
{
    if (index < sizeof(s_listen_bank) / sizeof(s_listen_bank[0]))
        return &s_listen_bank[index];
    return NULL;
}

uint8_t tc_get_listen_count(void)
{
    return sizeof(s_listen_bank) / sizeof(s_listen_bank[0]);
}

const naming_question_t *tc_get_naming_question(uint8_t index)
{
    if (index < sizeof(s_naming_bank) / sizeof(s_naming_bank[0]))
        return &s_naming_bank[index];
    return NULL;
}

uint8_t tc_get_naming_count(void)
{
    return sizeof(s_naming_bank) / sizeof(s_naming_bank[0]);
}

const sentence_question_t *tc_get_sentence_question(uint8_t index)
{
    if (index < sizeof(s_sentence_bank) / sizeof(s_sentence_bank[0]))
        return &s_sentence_bank[index];
    return NULL;
}

uint8_t tc_get_sentence_count(void)
{
    return sizeof(s_sentence_bank) / sizeof(s_sentence_bank[0]);
}

void tc_advance_question(void)
{
    tc_next_question();
}

/* ================================================================
 *  History (TODO: M7 migrate to storage module)
 * ================================================================ */

void tc_history_add(screen_id_t module, uint8_t correct, uint8_t total, uint32_t elapsed)
{
    if (s_history_count >= MAX_HISTORY) {
        /* Shift all records up by one, dropping oldest */
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            s_history[i] = s_history[i + 1];
        }
        s_history_count = MAX_HISTORY - 1;
    }
    s_history[s_history_count].module    = module;
    s_history[s_history_count].correct   = correct;
    s_history[s_history_count].total     = total;
    s_history[s_history_count].elapsed_s = elapsed;
    s_history[s_history_count].timestamp = 0;  /* M7: replace with RTC time once SNTP available */
    s_history_count++;
}

void tc_history_clear(void)
{
    s_history_count = 0;
    memset(s_history, 0, sizeof(s_history));
}

uint8_t tc_history_count(void)
{
    return s_history_count;
}

history_record_t *tc_history_get(uint8_t index)
{
    if (index < s_history_count) return &s_history[index];
    return NULL;
}
