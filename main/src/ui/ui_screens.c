#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "bsp/esp32_p4_nano.h"
#include "ui/ui_screens.h"
#include "ui/ui_theme.h"
#include "ui/ui_font.h"
#include "ui/ui_nav.h"
#include "ui/ui_components.h"
#include "app/app_events.h"
#include "app/app_tasks.h"
#include "app/training_controller.h"

static const char *TAG = "UI_SCR";

/* ================================================================
 *  Demo data (M2 in-memory, M7 replaced by storage)
 * ================================================================ */

static struct {
    char name[32];
    int  total_sessions;
} s_patients[5];

static int s_patient_count = 0;
static lv_timer_t *s_time_timer = NULL;

static void init_demo_patients(void)
{
    if (s_patient_count > 0) return;
    strcpy(s_patients[0].name, "张三");
    s_patients[0].total_sessions = 12;
    strcpy(s_patients[1].name, "李四");
    s_patients[1].total_sessions = 5;
    strcpy(s_patients[2].name, "王五");
    s_patients[2].total_sessions = 0;
    s_patient_count = 3;
}

static int s_selected_patient = -1;

screen_id_t ui_get_pending_module(void);

static screen_id_t s_pending_module = SCREEN_HOME;

screen_id_t ui_get_pending_module(void) { return s_pending_module; }

/* ================================================================
 *  Patient Select
 * ================================================================ */

static void on_new_patient(lv_event_t *e)
{
    if (s_patient_count >= 5) {
        ui_toast_show("最多5个用户");
        return;
    }
    char name[32];
    snprintf(name, sizeof(name), "用户%d", s_patient_count + 1);
    strcpy(s_patients[s_patient_count].name, name);
    s_patients[s_patient_count].total_sessions = 0;
    s_patient_count++;

    /* rebuild screen */
    ui_nav_push(SCREEN_PATIENT_SELECT);
}

static void on_select_patient(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_selected_patient = idx;
    ui_nav_push(SCREEN_HOME);
}

void ui_create_patient_select(lv_obj_t *root)
{
    init_demo_patients();

    ui_top_bar_create(root, "声语同行", false, NULL);

    lv_obj_t *cont = lv_obj_create(root);
    lv_obj_set_size(cont, lv_pct(90), lv_pct(70));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* patient list */
    for (int i = 0; i < s_patient_count; i++) {
        lv_obj_t *card = ui_card_create(cont, lv_pct(90), 56);
        lv_obj_set_style_pad_hor(card, TH_SPACING_LG, 0);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, s_patients[i].name);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_text_font(name, ui_font_get(TH_FONT_MD), 0);

        char info[40];
        snprintf(info, sizeof(info), "训练 %d 次", s_patients[i].total_sessions);
        lv_obj_t *sess = lv_label_create(card);
        lv_label_set_text(sess, info);
        lv_obj_align(sess, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_color(sess, TH_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(sess, ui_font_get(TH_FONT_SM), 0);

        lv_obj_add_event_cb(card, on_select_patient, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* spacer */
    lv_obj_t *sp = lv_obj_create(cont);
    lv_obj_set_size(sp, 1, 12);
    lv_obj_set_style_bg_opa(sp, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp, 0, 0);

    /* new patient button */
    ui_big_button_create(cont, "＋ 新建用户", lv_pct(80), TH_BTN_H_LG,
                         TH_PRIMARY, on_new_patient);
}

/* ================================================================
 *  Home
 * ================================================================ */

static void on_module_click(lv_event_t *e)
{
    screen_id_t module = (screen_id_t)(uintptr_t)lv_event_get_user_data(e);
    s_pending_module = module;
    payload_module_selected_t *p = malloc(sizeof(payload_module_selected_t));
    if (!p) { ESP_LOGE(TAG, "OOM module_select"); return; }
    p->module = module;
    app_msg_t msg = {
        .type = UI_EVT_MODULE_SELECTED, .session_id = 0,
        .payload_len = sizeof(payload_module_selected_t), .payload = p,
        .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

static void on_go_settings(lv_event_t *e)
{
    (void)e;
    ui_nav_push(SCREEN_SETTINGS);
}

static void on_go_history(lv_event_t *e)
{
    (void)e;
    ui_nav_push(SCREEN_HISTORY);
}

static void time_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *label = (lv_obj_t *)timer->user_data;
    if (!lv_obj_is_valid(label)) {
        lv_timer_del(timer);
        if (s_time_timer == timer) s_time_timer = NULL;
        return;
    }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char buf[32];
    if (tm->tm_year < 120) {  /* year < 2020 → time not synced */
        lv_label_set_text(label, "--:--");
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
        lv_label_set_text(label, buf);
    }
}

void ui_create_home(lv_obj_t *root)
{
    ui_top_bar_create(root, "首页", false, NULL);

    /* patient + time info bar */
    lv_obj_t *info = lv_obj_create(root);
    lv_obj_set_size(info, lv_pct(100), 36);
    lv_obj_set_pos(info, 0, 50);
    lv_obj_set_style_bg_color(info, TH_PRIMARY_BG, 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(info, 0, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_pad_hor(info, TH_SPACING_LG, 0);

    if (s_selected_patient >= 0 && s_selected_patient < s_patient_count) {
        lv_obj_t *pn = lv_label_create(info);
        lv_label_set_text(pn, s_patients[s_selected_patient].name);
        lv_obj_align(pn, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_text_color(pn, TH_PRIMARY, 0);
        lv_obj_set_style_text_font(pn, ui_font_get(TH_FONT_SM), 0);
    }

    lv_obj_t *clock = lv_label_create(info);
    lv_label_set_text(clock, "--:--");
    lv_obj_align(clock, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(clock, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(clock, ui_font_get(TH_FONT_SM), 0);
    if (s_time_timer) { lv_timer_del(s_time_timer); s_time_timer = NULL; }
    s_time_timer = lv_timer_create(time_timer_cb, 1000, clock);

    /* 4 training module buttons (2x2 grid) */
    lv_coord_t btn_w = 150;
    lv_coord_t btn_h = 120;
    lv_coord_t start_y = 110;
    lv_coord_t gap = 16;
    lv_coord_t left_x = (720 - btn_w * 2 - gap) / 2;

    struct { const char *name; lv_color_t color; screen_id_t module; } mods[] = {
        {"听理解训练", TH_MODULE_LISTEN,   SCREEN_LISTEN_TRAINING},
        {"句子构建",   TH_MODULE_SENTENCE, SCREEN_SENTENCE_BUILDING},
        {"命名训练",   TH_MODULE_NAMING,   SCREEN_NAMING_TRAINING},
        {"发音训练",   TH_MODULE_PRON,     SCREEN_PRONUNCIATION_TRAINING},
    };

    for (int i = 0; i < 4; i++) {
        lv_coord_t row = i / 2;
        lv_coord_t col = i % 2;
        lv_obj_t *btn = lv_btn_create(root);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, left_x + col * (btn_w + gap), start_y + row * (btn_h + gap));
        lv_obj_set_style_radius(btn, TH_BTN_RADIUS, 0);
        lv_obj_set_style_bg_color(btn, mods[i].color, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 4, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, mods[i].name);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, TH_TEXT_ON_PRIMARY, 0);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);

        lv_obj_add_event_cb(btn, on_module_click, LV_EVENT_CLICKED,
                           (void *)(uintptr_t)mods[i].module);
    }

    /* bottom buttons */
    lv_coord_t btm_y = start_y + 2 * (btn_h + gap) + 20;
    lv_coord_t btm_w = (720 - gap * 3) / 2;

    lv_obj_t *hist_btn = ui_big_button_create(root, "历史记录", btm_w, TH_BTN_H_MD,
                                              TH_TEXT_SECONDARY, on_go_history);
    lv_obj_set_pos(hist_btn, gap, btm_y);

    lv_obj_t *set_btn = ui_big_button_create(root, "设置", btm_w, TH_BTN_H_MD,
                                             TH_TEXT_SECONDARY, on_go_settings);
    lv_obj_set_pos(set_btn, gap * 2 + btm_w, btm_y);
}

/* ================================================================
 *  Settings
 * ================================================================ */

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_nav_back();
}

static void on_brightness_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    bsp_display_brightness_set(val);
}

static void on_volume_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    payload_volume_t *p = malloc(sizeof(payload_volume_t));
    if (p) {
        p->volume = val;
        app_msg_t msg = {
            .type = AUDIO_CMD_SET_VOLUME, .session_id = 0,
            .payload_len = sizeof(payload_volume_t), .payload = p,
            .error_code = 0,
        };
        queue_send_or_free(g_audio_queue, &msg);
    }
}

void ui_create_settings(lv_obj_t *root)
{
    ui_top_bar_create(root, "设置", true, on_back);

    lv_obj_t *cont = lv_obj_create(root);
    lv_obj_set_size(cont, lv_pct(90), lv_pct(80));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    lv_coord_t y = 0;
    lv_coord_t row_h = 52;

    /* ---- Brightness ---- */
    lv_obj_t *bl = lv_label_create(cont);
    lv_label_set_text(bl, "屏幕亮度");
    lv_obj_set_pos(bl, 0, y);
    lv_obj_set_style_text_font(bl, ui_font_get(TH_FONT_MD), 0);

    lv_obj_t *b_slider = lv_slider_create(cont);
    lv_obj_set_size(b_slider, 200, 10);
    lv_obj_set_pos(b_slider, 220, y + 8);
    lv_slider_set_range(b_slider, 10, 100);
    lv_slider_set_value(b_slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(b_slider, on_brightness_change, LV_EVENT_VALUE_CHANGED, NULL);
    y += row_h + TH_SPACING_SM;

    /* ---- Volume (placeholder) ---- */
    lv_obj_t *vl = lv_label_create(cont);
    lv_label_set_text(vl, "音量");
    lv_obj_set_pos(vl, 0, y);
    lv_obj_set_style_text_font(vl, ui_font_get(TH_FONT_MD), 0);

    lv_obj_t *v_slider = lv_slider_create(cont);
    lv_obj_set_size(v_slider, 200, 10);
    lv_obj_set_pos(v_slider, 220, y + 8);
    lv_slider_set_range(v_slider, 0, 100);
    lv_slider_set_value(v_slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(v_slider, on_volume_change, LV_EVENT_VALUE_CHANGED, NULL);
    y += row_h + TH_SPACING_SM;

    /* ---- Sleep timeout (placeholder) ---- */
    lv_obj_t *sl = lv_label_create(cont);
    lv_label_set_text(sl, "自动息屏");
    lv_obj_set_pos(sl, 0, y);
    lv_obj_set_style_text_font(sl, ui_font_get(TH_FONT_MD), 0);

    lv_obj_t *sv = lv_label_create(cont);
    lv_label_set_text(sv, "300 秒");
    lv_obj_set_pos(sv, 220, y);
    lv_obj_set_style_text_color(sv, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(sv, ui_font_get(TH_FONT_MD), 0);
    y += row_h + TH_SPACING_SM;

    /* ---- Dev mode (placeholder) ---- */
    lv_obj_t *dl = lv_label_create(cont);
    lv_label_set_text(dl, "开发者模式");
    lv_obj_set_pos(dl, 0, y);
    lv_obj_set_style_text_font(dl, ui_font_get(TH_FONT_MD), 0);

    lv_obj_t *dv = lv_label_create(cont);
    lv_label_set_text(dv, "关闭");
    lv_obj_set_pos(dv, 220, y);
    lv_obj_set_style_text_color(dv, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(dv, ui_font_get(TH_FONT_MD), 0);
    y += row_h + TH_SPACING_XL;

    /* ---- About ---- */
    lv_obj_t *al = lv_label_create(cont);
    lv_label_set_text(al, "关于设备");
    lv_obj_set_pos(al, 0, y);
    lv_obj_set_style_text_font(al, ui_font_get(TH_FONT_MD), 0);

    lv_obj_t *av = lv_label_create(cont);
    lv_label_set_text(av, "v2.0.0");
    lv_obj_set_pos(av, 220, y);
    lv_obj_set_style_text_color(av, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(av, ui_font_get(TH_FONT_MD), 0);
}

/* ================================================================
 *  M3: Listening Comprehension Training
 * ================================================================ */

static lv_obj_t   *s_choice_btns[4];
static lv_obj_t   *s_choice_labels[4];
static lv_obj_t   *s_listen_prompt;
static lv_obj_t   *s_listen_progress;
static uint8_t     s_listen_q_idx;
static uint8_t     s_listen_total;
static lv_timer_t *s_feedback_timer;  /* singleton guard */

/* Called by UI task to update question in-place after first render */
void ui_listen_update_question(uint8_t q_idx)
{
    const listen_question_t *q = tc_get_listen_question(q_idx);
    if (!q) return;
    s_listen_q_idx = q_idx;

    if (s_listen_prompt && lv_obj_is_valid(s_listen_prompt)) {
        lv_label_set_text(s_listen_prompt, q->prompt);
    }
    for (int i = 0; i < 4; i++) {
        if (s_choice_labels[i] && lv_obj_is_valid(s_choice_labels[i])) {
            lv_label_set_text(s_choice_labels[i], q->choices[i]);
        }
        if (s_choice_btns[i] && lv_obj_is_valid(s_choice_btns[i])) {
            lv_obj_clear_state(s_choice_btns[i], LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(s_choice_btns[i], lv_color_hex(0xE3F2FD), 0);
        }
    }
    if (s_listen_progress && lv_obj_is_valid(s_listen_progress)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "第 %d/%d 题", q_idx + 1, s_listen_total);
        lv_label_set_text(s_listen_progress, buf);
    }
}

static void on_listen_choice(lv_event_t *e)
{
    uint8_t choice = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 4; i++) {
        lv_obj_add_state(s_choice_btns[i], LV_STATE_DISABLED);
    }
    payload_answer_t *p = malloc(sizeof(payload_answer_t));
    if (!p) { ESP_LOGE(TAG, "OOM answer"); return; }
    p->question_idx = s_listen_q_idx;
    p->answer_idx   = choice;
    app_msg_t msg = {
        .type = UI_EVT_ANSWER_SELECTED, .session_id = 0,
        .payload_len = sizeof(payload_answer_t), .payload = p,
        .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

static void listen_feedback_timer_cb(lv_timer_t *timer)
{
    s_feedback_timer = NULL;
    lv_timer_del(timer);
    app_msg_t msg = {
        .type = UI_EVT_FEEDBACK_DONE,
        .session_id = 0,
        .payload_len = 0,
        .payload = NULL,
        .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

void ui_listen_show_feedback(void *fb_payload)
{
    if (!fb_payload) return;
    if (!s_choice_btns[0] || !lv_obj_is_valid(s_choice_btns[0])) return;

    payload_feedback_t *fb = (payload_feedback_t *)fb_payload;
    for (int i = 0; i < 4; i++) {
        if (s_choice_btns[i] && lv_obj_is_valid(s_choice_btns[i])) {
            if (i == fb->correct_idx) {
                lv_obj_set_style_bg_color(s_choice_btns[i], TH_SUCCESS, 0);
            } else if (i == fb->answer_idx && !fb->correct) {
                lv_obj_set_style_bg_color(s_choice_btns[i], TH_DANGER, 0);
            }
        }
    }
    if (s_feedback_timer) { lv_timer_del(s_feedback_timer); }
    s_feedback_timer = lv_timer_create(listen_feedback_timer_cb, 1500, NULL);
}

void ui_create_listen_training(lv_obj_t *root)
{
    ui_top_bar_create(root, "听理解训练", true, on_back);

    const listen_question_t *q = tc_get_listen_question(0);
    if (!q) {
        lv_obj_t *lbl = lv_label_create(root);
        lv_label_set_text(lbl, "暂无题目");
        lv_obj_center(lbl);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
        return;
    }

    s_listen_q_idx = 0;
    s_listen_total = tc_get_listen_count();

    /* Prompt */
    s_listen_prompt = lv_label_create(root);
    lv_label_set_text(s_listen_prompt, q->prompt);
    lv_obj_set_pos(s_listen_prompt, 20, 60);
    lv_obj_set_width(s_listen_prompt, 680);
    lv_label_set_long_mode(s_listen_prompt, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_listen_prompt, ui_font_get(TH_FONT_MD), 0);
    lv_obj_set_style_text_color(s_listen_prompt, TH_TEXT, 0);

    /* 2x2 grid */
    lv_coord_t btn_w = 300, btn_h = 140, gap = 20;
    lv_coord_t sx = (720 - btn_w * 2 - gap) / 2, sy = 170;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(root);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, sx + (i % 2) * (btn_w + gap),
                       sy + (i / 2) * (btn_h + gap));
        lv_obj_set_style_radius(btn, TH_BTN_RADIUS, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xE3F2FD), 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_color(btn, TH_PRIMARY, 0);
        lv_obj_set_style_shadow_width(btn, 2, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, q->choices[i]);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, TH_TEXT, 0);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
        lv_obj_add_event_cb(btn, on_listen_choice, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_choice_btns[i] = btn;
        s_choice_labels[i] = lbl;
    }

    /* Progress */
    char buf[32];
    snprintf(buf, sizeof(buf), "第 1/%d 题", s_listen_total);
    s_listen_progress = lv_label_create(root);
    lv_label_set_text(s_listen_progress, buf);
    lv_obj_align(s_listen_progress, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(s_listen_progress, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_listen_progress, ui_font_get(TH_FONT_SM), 0);
}

/* ================================================================
 *  M3: Naming Training (UI only, no scoring)
 * ================================================================ */

static uint8_t   s_naming_q_idx;
static uint8_t   s_naming_total;
static lv_obj_t *s_naming_status;
static lv_obj_t *s_naming_hint;
static lv_obj_t *s_naming_img_text;
static lv_obj_t *s_naming_progress;

void ui_naming_update_question(uint8_t q_idx)
{
    const naming_question_t *q = tc_get_naming_question(q_idx);
    if (!q) return;
    s_naming_q_idx = q_idx;

    if (s_naming_hint && lv_obj_is_valid(s_naming_hint)) {
        lv_label_set_text(s_naming_hint, q->hint);
    }
    if (s_naming_status && lv_obj_is_valid(s_naming_status)) {
        lv_label_set_text(s_naming_status, "按住按钮开始录音");
    }
    if (s_naming_progress && lv_obj_is_valid(s_naming_progress)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "第 %d/%d 题", q_idx + 1, s_naming_total);
        lv_label_set_text(s_naming_progress, buf);
    }
}

void ui_naming_show_ready(void *payload)
{
    /* M4.2: recording finished — update status for replay */
    (void)payload;  /* recording ownership is with app_task for now */
    if (s_naming_status && lv_obj_is_valid(s_naming_status)) {
        lv_label_set_text(s_naming_status, "录音完成，可回放");
    }
}

static void send_record_event(bool pressed)
{
    payload_record_t *p = malloc(sizeof(payload_record_t));
    if (!p) { ESP_LOGE(TAG, "OOM record"); return; }
    p->pressed = pressed;
    app_msg_t msg = {
        .type = UI_EVT_RECORD_PRESSED, .session_id = 0,
        .payload_len = sizeof(payload_record_t), .payload = p,
        .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

static void on_record_press(lv_event_t *e)
{
    send_record_event(true);
    if (s_naming_status && lv_obj_is_valid(s_naming_status))
        lv_label_set_text(s_naming_status, "正在录音...");
}

static void on_record_release(lv_event_t *e)
{
    send_record_event(false);
    if (s_naming_status && lv_obj_is_valid(s_naming_status))
        lv_label_set_text(s_naming_status, "语音识别将在 M5 接入");
}

static void on_naming_replay(lv_event_t *e)
{
    app_msg_t msg = {
        .type = UI_EVT_REPLAY_PRESSED, .session_id = 0,
        .payload_len = 0, .payload = NULL, .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

static void on_naming_next(lv_event_t *e)
{
    app_msg_t msg = {
        .type = UI_EVT_FEEDBACK_DONE, .session_id = 0,
        .payload_len = 0, .payload = NULL, .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

void ui_create_naming_training(lv_obj_t *root)
{
    ui_top_bar_create(root, "命名训练", true, on_back);

    const naming_question_t *q = tc_get_naming_question(0);
    if (!q) {
        lv_obj_t *lbl = lv_label_create(root);
        lv_label_set_text(lbl, "暂无题目");
        lv_obj_center(lbl);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
        return;
    }

    s_naming_q_idx = 0;
    s_naming_total = tc_get_naming_count();

    /* Image placeholder */
    lv_obj_t *img_area = lv_obj_create(root);
    lv_obj_set_size(img_area, 320, 320);
    lv_obj_align(img_area, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(img_area, lv_color_hex(0xE8EAF6), 0);
    lv_obj_set_style_border_width(img_area, 2, 0);
    lv_obj_set_style_border_color(img_area, TH_PRIMARY, 0);
    lv_obj_set_style_radius(img_area, 16, 0);

    s_naming_img_text = lv_label_create(img_area);
    lv_label_set_text(s_naming_img_text, "图片区域\n(M7 接入)");
    lv_obj_center(s_naming_img_text);
    lv_obj_set_style_text_color(s_naming_img_text, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_naming_img_text, ui_font_get(TH_FONT_MD), 0);

    /* Hint */
    s_naming_hint = lv_label_create(root);
    lv_label_set_text(s_naming_hint, q->hint);
    lv_obj_align(s_naming_hint, LV_ALIGN_TOP_MID, 0, 420);
    lv_obj_set_style_text_color(s_naming_hint, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_naming_hint, ui_font_get(TH_FONT_SM), 0);

    /* Status */
    s_naming_status = lv_label_create(root);
    lv_label_set_text(s_naming_status, "按住按钮开始录音");
    lv_obj_align(s_naming_status, LV_ALIGN_TOP_MID, 0, 460);
    lv_obj_set_style_text_font(s_naming_status, ui_font_get(TH_FONT_SM), 0);

    /* Record button */
    lv_obj_t *rec_btn = lv_btn_create(root);
    lv_obj_set_size(rec_btn, 160, 48);
    lv_obj_align(rec_btn, LV_ALIGN_TOP_MID, 0, 510);
    lv_obj_set_style_radius(rec_btn, 24, 0);
    lv_obj_set_style_bg_color(rec_btn, TH_DANGER, 0);
    lv_obj_set_style_border_width(rec_btn, 0, 0);
    lv_obj_t *rec_lbl = lv_label_create(rec_btn);
    lv_label_set_text(rec_lbl, "按住录音");
    lv_obj_center(rec_lbl);
    lv_obj_set_style_text_color(rec_lbl, TH_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(rec_lbl, ui_font_get(TH_FONT_MD), 0);
    lv_obj_add_event_cb(rec_btn, on_record_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(rec_btn, on_record_release, LV_EVENT_RELEASED, NULL);

    /* Bottom buttons */
    lv_coord_t btm_w = 200, btm_y = 620;
    ui_big_button_create(root, "回放", btm_w, TH_BTN_H_MD, TH_TEXT_SECONDARY, on_naming_replay);
    lv_obj_set_pos(lv_obj_get_child(root, lv_obj_get_child_cnt(root) - 1),
                   (720 - btm_w * 2 - 20) / 2, btm_y);

    ui_big_button_create(root, "下一题", btm_w, TH_BTN_H_MD, TH_PRIMARY, on_naming_next);
    lv_obj_set_pos(lv_obj_get_child(root, lv_obj_get_child_cnt(root) - 1),
                   (720 - btm_w * 2 - 20) / 2 + btm_w + 20, btm_y);

    /* Progress */
    char buf[32];
    snprintf(buf, sizeof(buf), "第 1/%d 题", s_naming_total);
    s_naming_progress = lv_label_create(root);
    lv_label_set_text(s_naming_progress, buf);
    lv_obj_align(s_naming_progress, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(s_naming_progress, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_naming_progress, ui_font_get(TH_FONT_SM), 0);
}

/* ================================================================
 *  M4: Sentence Building Screen
 * ================================================================ */

#define SENTENCE_SLOT_W  130
#define SENTENCE_SLOT_H   50
#define SENTENCE_BTN_W   130
#define SENTENCE_BTN_H    50
#define SENTENCE_GAP      12

static const sentence_question_t *s_sent_q;
static uint8_t   s_sent_q_idx;
static lv_obj_t *s_sent_word_btns[SENTENCE_MAX_WORDS];
static lv_obj_t *s_sent_slot_btns[SENTENCE_MAX_WORDS];
static int8_t   s_sent_selected[SENTENCE_MAX_WORDS]; /* -1 = empty, else word index */
static uint8_t  s_sent_selected_count;
static uint8_t  s_sent_total;
static lv_obj_t *s_sent_progress;
static lv_obj_t *s_sent_hint;

static void sent_update_slots(void)
{
    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) {
        if (s_sent_slot_btns[i] && lv_obj_is_valid(s_sent_slot_btns[i])) {
            if (s_sent_selected[i] >= 0 && s_sent_q) {
                lv_label_set_text(lv_obj_get_child(s_sent_slot_btns[i], 0),
                                  s_sent_q->words[s_sent_selected[i]]);
                lv_obj_clear_state(s_sent_slot_btns[i], LV_STATE_DISABLED);
                lv_obj_set_style_bg_color(s_sent_slot_btns[i], TH_PRIMARY, 0);
                lv_obj_set_style_text_color(lv_obj_get_child(s_sent_slot_btns[i], 0),
                                            lv_color_white(), 0);
            } else {
                lv_label_set_text(lv_obj_get_child(s_sent_slot_btns[i], 0), "");
                lv_obj_set_style_bg_color(s_sent_slot_btns[i],
                                          lv_color_hex(0xE5E7EB), 0);
                lv_obj_set_style_text_color(lv_obj_get_child(s_sent_slot_btns[i], 0),
                                            lv_color_hex(0x9CA3AF), 0);
            }
        }
    }
    /* Update checkmark/clear widgets for slots */
    if (!s_sent_q) return;
    for (int i = 0; i < s_sent_q->word_count; i++) {
        if (s_sent_word_btns[i] && lv_obj_is_valid(s_sent_word_btns[i])) {
            bool used = false;
            for (int j = 0; j < SENTENCE_MAX_WORDS; j++) {
                if (s_sent_selected[j] == i) { used = true; break; }
            }
            if (used) {
                lv_obj_add_state(s_sent_word_btns[i], LV_STATE_DISABLED);
                lv_obj_set_style_bg_color(s_sent_word_btns[i],
                                          lv_color_hex(0xD1D5DB), 0);
                lv_obj_set_style_text_color(lv_obj_get_child(s_sent_word_btns[i], 0),
                                            lv_color_hex(0x9CA3AF), 0);
            } else {
                lv_obj_clear_state(s_sent_word_btns[i], LV_STATE_DISABLED);
                lv_obj_set_style_bg_color(s_sent_word_btns[i], TH_PRIMARY, 0);
                lv_obj_set_style_text_color(lv_obj_get_child(s_sent_word_btns[i], 0),
                                            lv_color_white(), 0);
            }
        }
    }
}

static void on_sent_word_click(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (lv_obj_has_state(btn, LV_STATE_DISABLED)) return;
    if (s_sent_selected_count >= s_sent_q->word_count) return;

    /* Find first empty slot */
    for (int i = 0; i < s_sent_q->word_count; i++) {
        if (s_sent_selected[i] == -1) {
            s_sent_selected[i] = (int8_t)idx;
            s_sent_selected_count++;
            break;
        }
    }
    sent_update_slots();
}

static void on_sent_slot_click(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_sent_selected[idx] == -1) return;
    s_sent_selected[idx] = -1;
    s_sent_selected_count--;
    sent_update_slots();
}

static void on_sent_clear(lv_event_t *e)
{
    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) s_sent_selected[i] = -1;
    s_sent_selected_count = 0;
    sent_update_slots();
}

static void sent_feedback_timer_cb(lv_timer_t *t)
{
    s_feedback_timer = NULL;
    lv_timer_del(t);
    app_msg_t msg = {
        .type = UI_EVT_FEEDBACK_DONE,
        .session_id = 0, .payload_len = 0, .payload = NULL, .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);
}

static void on_sent_confirm(lv_event_t *e)
{
    if (!s_sent_q || s_sent_selected_count != s_sent_q->word_count) return;

    /* Disable all buttons */
    for (int i = 0; i < s_sent_q->word_count; i++) {
        if (s_sent_word_btns[i]) lv_obj_add_state(s_sent_word_btns[i], LV_STATE_DISABLED);
        if (s_sent_slot_btns[i]) lv_obj_add_state(s_sent_slot_btns[i], LV_STATE_DISABLED);
    }
    /* Highlight slots green/red based on correct */
    for (int i = 0; i < s_sent_q->word_count; i++) {
        lv_color_t c = (s_sent_selected[i] == s_sent_q->correct_order[i])
                       ? TH_SUCCESS : TH_DANGER;
        lv_obj_set_style_bg_color(s_sent_slot_btns[i], c, 0);
    }

    /* Send answer */
    payload_sentence_answer_t *payload = malloc(sizeof(payload_sentence_answer_t));
    if (!payload) { ESP_LOGE(TAG, "OOM sent_confirm"); return; }
    payload->q_idx = s_sent_q_idx;
    payload->selected_count = s_sent_q->word_count;
    for (int i = 0; i < s_sent_q->word_count; i++) {
        payload->selected_order[i] = (uint8_t)s_sent_selected[i];
    }

    app_msg_t msg = {
        .type = UI_EVT_SENTENCE_CONFIRM,
        .session_id = 0,
        .payload_len = sizeof(payload_sentence_answer_t),
        .payload = payload,
        .error_code = 0,
    };
    queue_send_or_free(g_app_queue, &msg);

    if (s_feedback_timer) { lv_timer_del(s_feedback_timer); }
    s_feedback_timer = lv_timer_create(sent_feedback_timer_cb, 1500, NULL);
}

void ui_create_sentence_building(lv_obj_t *root)
{
    ui_top_bar_create(root, "句子构建", true, on_back);

    s_sent_q = tc_get_sentence_question(0);
    s_sent_total = tc_get_sentence_count();
    s_sent_selected_count = 0;
    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) {
        s_sent_selected[i] = -1;
        s_sent_word_btns[i] = NULL;
        s_sent_slot_btns[i] = NULL;
    }

    if (!s_sent_q) return;

    /* Hint */
    s_sent_hint = lv_label_create(root);
    lv_label_set_text(s_sent_hint, s_sent_q->hint);
    lv_obj_set_style_text_color(s_sent_hint, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_sent_hint, ui_font_get(TH_FONT_SM), 0);
    lv_obj_align(s_sent_hint, LV_ALIGN_TOP_MID, 0, 65);

    /* Slot bar — create all SENTENCE_MAX_WORDS slots upfront, show/hide per question */
    lv_coord_t slots_total_w = SENTENCE_MAX_WORDS * SENTENCE_SLOT_W
                              + (SENTENCE_MAX_WORDS - 1) * SENTENCE_GAP;
    lv_coord_t slot_start_x = (720 - slots_total_w) / 2;
    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) {
        lv_obj_t *slot = lv_btn_create(root);
        lv_obj_set_size(slot, SENTENCE_SLOT_W, SENTENCE_SLOT_H);
        lv_obj_set_pos(slot, slot_start_x + i * (SENTENCE_SLOT_W + SENTENCE_GAP), 100);
        lv_obj_set_style_bg_color(slot, lv_color_hex(0xE5E7EB), 0);
        lv_obj_set_style_border_width(slot, 2, 0);
        lv_obj_set_style_border_color(slot, lv_color_hex(0xD1D5DB), 0);
        lv_obj_set_style_radius(slot, 8, 0);
        lv_obj_add_event_cb(slot, on_sent_slot_click, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_t *lbl = lv_label_create(slot);
        lv_label_set_text(lbl, "");
        lv_obj_center(lbl);
        s_sent_slot_btns[i] = slot;
        if (i >= s_sent_q->word_count) lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
    }

    /* Word buttons — 2 rows of 3, create all up to SENTENCE_MAX_WORDS */
    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) {
        int row = (i < 3) ? 0 : 1;
        int col = (i < 3) ? i : (i - 3);
        lv_coord_t rw = 3 * SENTENCE_BTN_W + 2 * SENTENCE_GAP;
        lv_coord_t rx = (720 - rw) / 2;

        lv_obj_t *btn = lv_btn_create(root);
        lv_obj_set_size(btn, SENTENCE_BTN_W, SENTENCE_BTN_H);
        lv_obj_set_pos(btn, rx + col * (SENTENCE_BTN_W + SENTENCE_GAP),
                       180 + row * (SENTENCE_BTN_H + SENTENCE_GAP));
        lv_obj_set_style_bg_color(btn, TH_PRIMARY, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_add_event_cb(btn, on_sent_word_click, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, (i < s_sent_q->word_count) ? s_sent_q->words[i] : "");
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
        lv_obj_center(lbl);
        s_sent_word_btns[i] = btn;
        if (i >= s_sent_q->word_count) lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }

    /* Bottom buttons */
    lv_coord_t btm_y = 180 + 2 * (SENTENCE_BTN_H + SENTENCE_GAP) + 20;
    lv_obj_t *clear_btn = ui_big_button_create(root, "清除", 140, TH_BTN_H_MD,
                                                TH_TEXT_SECONDARY, on_sent_clear);
    lv_obj_set_pos(clear_btn, (720 - 140*2 - 20) / 2, btm_y);

    lv_obj_t *confirm_btn = ui_big_button_create(root, "确认提交", 140, TH_BTN_H_MD,
                                                  TH_PRIMARY, on_sent_confirm);
    lv_obj_set_pos(confirm_btn, (720 - 140*2 - 20) / 2 + 140 + 20, btm_y);

    /* Progress */
    char buf[32];
    snprintf(buf, sizeof(buf), "第 1/%d 题", s_sent_total);
    s_sent_progress = lv_label_create(root);
    lv_label_set_text(s_sent_progress, buf);
    lv_obj_align(s_sent_progress, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(s_sent_progress, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_sent_progress, ui_font_get(TH_FONT_SM), 0);
}

void ui_sentence_update_question(uint8_t q_idx)
{
    /* Guard: skip if screen not yet created (q_idx > 0 before first push) */
    if (!s_sent_hint || !lv_obj_is_valid(s_sent_hint)) return;

    s_sent_q = tc_get_sentence_question(q_idx);
    if (!s_sent_q) return;
    s_sent_q_idx = q_idx;
    s_sent_selected_count = 0;
    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) s_sent_selected[i] = -1;

    lv_label_set_text(s_sent_hint, s_sent_q->hint);

    for (int i = 0; i < SENTENCE_MAX_WORDS; i++) {
        if (s_sent_word_btns[i]) {
            if (i < s_sent_q->word_count) {
                lv_label_set_text(lv_obj_get_child(s_sent_word_btns[i], 0),
                                  s_sent_q->words[i]);
                lv_obj_clear_flag(s_sent_word_btns[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_state(s_sent_word_btns[i], LV_STATE_DISABLED);
            } else {
                lv_obj_add_flag(s_sent_word_btns[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (s_sent_slot_btns[i]) {
            if (i < s_sent_q->word_count) {
                lv_obj_clear_flag(s_sent_slot_btns[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_state(s_sent_slot_btns[i], LV_STATE_DISABLED);
            } else {
                lv_obj_add_flag(s_sent_slot_btns[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    sent_update_slots();

    char buf[32];
    snprintf(buf, sizeof(buf), "第 %d/%d 题", q_idx + 1, s_sent_total);
    lv_label_set_text(s_sent_progress, buf);
}

/* ================================================================
 *  M4: Pronunciation Training (placeholder)
 * ================================================================ */

void ui_create_pronunciation_training(lv_obj_t *root)
{
    ui_top_bar_create(root, "发音训练", true, on_back);

    lv_obj_t *lbl = lv_label_create(root);
    lv_label_set_text(lbl, "发音训练\nM4 接入\n\n基于语音识别进行发音评估");
    lv_obj_center(lbl);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
}

/* ================================================================
 *  M3: Training Result Page
 * ================================================================ */

static uint8_t    s_result_correct;
static uint8_t    s_result_total;
static uint32_t   s_result_elapsed;
static screen_id_t s_result_module;

void ui_set_result_data(uint8_t correct, uint8_t total, uint32_t elapsed, int module)
{
    s_result_correct = correct;
    s_result_total   = total;
    s_result_elapsed = elapsed;
    s_result_module  = (screen_id_t)module;
}

static void on_result_home(lv_event_t *e)
{
    while (ui_nav_current() != SCREEN_PATIENT_SELECT) {
        ui_nav_back();
    }
}

static void on_result_retry(lv_event_t *e)
{
    /* Pop back to HOME to avoid nav stack overflow across retries */
    while (ui_nav_current() != SCREEN_HOME) {
        ui_nav_back();
    }
    payload_module_selected_t *p = malloc(sizeof(payload_module_selected_t));
    if (!p) { ESP_LOGE(TAG, "OOM retry"); return; }
    p->module = s_result_module;
    app_msg_t msg = {
        .type = UI_EVT_MODULE_SELECTED, .session_id = 0,
        .payload_len = sizeof(payload_module_selected_t), .payload = p,
    };
    queue_send_or_free(g_app_queue, &msg);
}

void ui_create_training_result(lv_obj_t *root)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d", s_result_correct, s_result_total);

    ui_top_bar_create(root, "训练完成", false, NULL);

    /* Big score text */
    lv_obj_t *score = lv_label_create(root);
    lv_label_set_text(score, buf);
    lv_obj_align(score, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_text_font(score, ui_font_get(TH_FONT_LG), 0);
    lv_obj_set_style_text_color(score, TH_PRIMARY, 0);

    /* Encouragement text */
    const char *enc = (s_result_correct == s_result_total) ? "太棒了！全对！"
                     : (s_result_correct > s_result_total / 2) ? "做得不错！"
                     : "继续加油！";
    lv_obj_t *enc_lbl = lv_label_create(root);
    lv_label_set_text(enc_lbl, enc);
    lv_obj_align(enc_lbl, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_set_style_text_font(enc_lbl, ui_font_get(TH_FONT_MD), 0);

    /* Elapsed time */
    snprintf(buf, sizeof(buf), "用时: %lu 秒", (unsigned long)s_result_elapsed);
    lv_obj_t *time_lbl = lv_label_create(root);
    lv_label_set_text(time_lbl, buf);
    lv_obj_align(time_lbl, LV_ALIGN_TOP_MID, 0, 230);
    lv_obj_set_style_text_color(time_lbl, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(time_lbl, ui_font_get(TH_FONT_SM), 0);

    /* Buttons */
    lv_obj_t *home_btn = ui_big_button_create(root, "返回首页", 280, TH_BTN_H_MD,
                                               TH_PRIMARY, on_result_home);
    lv_obj_align(home_btn, LV_ALIGN_BOTTOM_MID, 0, -100);

    lv_obj_t *retry_btn = ui_big_button_create(root, "再来一轮", 280, TH_BTN_H_MD,
                                                TH_SUCCESS, on_result_retry);
    lv_obj_align(retry_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
}

/* ================================================================
 *  M4: History Screen (Step C — stub for now)
 * ================================================================ */

static const char *history_module_name(screen_id_t m)
{
    switch (m) {
        case SCREEN_LISTEN_TRAINING:   return "听理解训练";
        case SCREEN_NAMING_TRAINING:   return "命名训练";
        case SCREEN_SENTENCE_BUILDING: return "句子构建";
        default:                       return "训练";
    }
}

static void on_history_clear(lv_event_t *e)
{
    tc_history_clear();
    lv_obj_t *list = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_clean(list);
    lv_obj_t *lbl = lv_label_create(list);
    lv_label_set_text(lbl, "暂无训练记录");
    lv_obj_set_style_text_color(lbl, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
    lv_obj_center(lbl);
    ui_toast_show("记录已清空");
}

void ui_create_history(lv_obj_t *root)
{
    ui_top_bar_create(root, "历史记录", true, on_back);

    uint8_t count = tc_history_count();

    char buf[64];
    snprintf(buf, sizeof(buf), "总训练次数: %d", count);
    lv_obj_t *stats = lv_label_create(root);
    lv_label_set_text(stats, buf);
    lv_obj_set_style_text_color(stats, TH_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(stats, ui_font_get(TH_FONT_MD), 0);
    lv_obj_align(stats, LV_ALIGN_TOP_MID, 0, 65);

    /* Scrollable list container */
    lv_obj_t *list = lv_obj_create(root);
    lv_obj_set_size(list, lv_pct(90), 600);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 95);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    if (count == 0) {
        lv_obj_t *lbl = lv_label_create(list);
        lv_label_set_text(lbl, "暂无训练记录");
        lv_obj_set_style_text_color(lbl, TH_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);
        lv_obj_center(lbl);
    } else {
        for (uint8_t i = count; i > 0; i--) {
            history_record_t *r = tc_history_get(i - 1);  /* newest first */
            if (!r) continue;

            lv_obj_t *card = lv_obj_create(list);
            lv_obj_set_size(card, lv_pct(100), 80);
            lv_obj_set_style_bg_color(card, lv_color_white(), 0);
            lv_obj_set_style_radius(card, 10, 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_border_color(card, lv_color_hex(0xE5E7EB), 0);
            lv_obj_set_style_pad_all(card, 12, 0);

            /* Module name + score */
            lv_obj_t *title = lv_label_create(card);
            snprintf(buf, sizeof(buf), "%s  %d/%d",
                     history_module_name(r->module), r->correct, r->total);
            lv_label_set_text(title, buf);
            lv_obj_set_style_text_color(title, TH_TEXT, 0);
            lv_obj_set_style_text_font(title, ui_font_get(TH_FONT_MD), 0);
            lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

            /* Elapsed time */
            lv_obj_t *time_lbl = lv_label_create(card);
            snprintf(buf, sizeof(buf), "%lus", (unsigned long)r->elapsed_s);
            lv_label_set_text(time_lbl, buf);
            lv_obj_set_style_text_color(time_lbl, TH_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_font(time_lbl, ui_font_get(TH_FONT_SM), 0);
            lv_obj_align(time_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

            /* Score indicator */
            lv_obj_t *score = lv_label_create(card);
            const char *icon = (r->correct == r->total) ? "✓" : "";
            snprintf(buf, sizeof(buf), "%s", icon);
            lv_label_set_text(score, buf);
            lv_obj_set_style_text_color(score,
                (r->correct == r->total) ? TH_SUCCESS : TH_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_font(score, ui_font_get(TH_FONT_MD), 0);
            lv_obj_align(score, LV_ALIGN_RIGHT_MID, -10, 0);
        }
    }

    /* Clear button */
    if (count > 0) {
        lv_obj_t *clear_btn = ui_big_button_create(root, "清空记录", 200, TH_BTN_H_MD,
                                                     TH_DANGER, NULL);
        lv_obj_add_event_cb(clear_btn, on_history_clear, LV_EVENT_CLICKED, (void *)list);
        lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    }
}
