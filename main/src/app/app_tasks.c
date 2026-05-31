#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"
#include "bsp/esp32_p4_nano.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "app/app_tasks.h"
#include "app/app_events.h"
#include "diagnostics/diag_console.h"
#include "app/training_controller.h"
#include "ui/ui_app.h"
#include "ui/ui_screens.h"
#include "ui/ui_nav.h"
#include "ui/ui_components.h"
#include "audio/audio_manager.h"
#include "audio/audio_prompts.h"
#include "esp_log.h"

void ui_task_main(void *arg)
{
    bool ok = bsp_display_start();
    if (!ok) {
        printf("[UI   ] FATAL: Display init failed\n");
        vTaskDelete(NULL);
        return;
    }
    bsp_display_brightness_init();
    bsp_display_backlight_on();
    bsp_display_brightness_set(100);
    printf("[UI   ] Display ready (720x1280 MIPI DSI)\n");

    esp_task_wdt_add(NULL);

    /* LVGL port has its own task on Core 1 calling lv_timer_handler().
     * All LVGL API calls from this task must hold the port lock. */
    lvgl_port_lock(-1);
    ui_app_start();
    lvgl_port_unlock();

    static screen_id_t s_current_module;

    app_msg_t msg;
    while (1) {
        esp_task_wdt_reset();

        if (xQueueReceive(g_ui_queue, &msg, 0) == pdTRUE) {
            lvgl_port_lock(-1);

            switch (msg.type) {
            case UI_CMD_RENDER_QUESTION: {
                payload_render_q_t *p = (payload_render_q_t *)msg.payload;
                if (p) {
                    screen_id_t mod = ui_get_pending_module();
                    s_current_module = mod;
                    if (p->q_idx == 0) {
                        ui_nav_push(mod);
                    }
                    if (mod == SCREEN_LISTEN_TRAINING) {
                        ui_listen_update_question(p->q_idx);
                    } else if (mod == SCREEN_NAMING_TRAINING) {
                        ui_naming_update_question(p->q_idx);
                    } else if (mod == SCREEN_SENTENCE_BUILDING) {
                        ui_sentence_update_question(p->q_idx);
                    }
                }
                break;
            }
            case UI_CMD_SHOW_FEEDBACK: {
                ui_listen_show_feedback(msg.payload);
                break;
            }
            case UI_CMD_SHOW_RESULT: {
                payload_result_t *p = (payload_result_t *)msg.payload;
                if (p) {
                    ui_set_result_data(p->correct_cnt, p->total,
                                      p->elapsed_s, s_current_module);
                    ui_nav_push(SCREEN_TRAINING_RESULT);
                }
                break;
            }
            case UI_CMD_RECORDING_READY:
            case APP_EVT_RECORDING_DONE: {
                /* M4.2: recording complete — update naming training UI */
                ui_naming_show_ready(msg.payload);
                break;
            }
            case APP_EVT_AUDIO_ERROR: {
                ui_toast_show("音频错误");
                break;
            }
            default:
                break;
            }

            lvgl_port_unlock();
            free(msg.payload);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ---- Recording helper: send RECORDING_DONE with ownership transfer ---- */
static void send_recording_done_or_free(audio_recording_t *rec)
{
    if (!rec) return;

    app_msg_t msg = {
        .type        = APP_EVT_RECORDING_DONE,
        .session_id  = 0,
        .payload_len = sizeof(audio_recording_t),
        .payload     = rec,
        .error_code  = (rec->stop_reason == AUDIO_REC_STOP_ERROR)
                        ? ESP_FAIL : 0,
    };

    if (xQueueSend(g_app_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE("AUDIO", "Failed to send RECORDING_DONE, freeing buffer");
        audio_recording_free(rec);
    }
}

void audio_task_main(void *arg)
{
    printf("[AUDIO] Audio task starting, init codec...\n");

    while (audio_init() != ESP_OK) {
        printf("[AUDIO] Audio init failed, retrying in 30 s...\n");
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
    printf("[AUDIO] Audio task ready, waiting on queue\n");

    app_msg_t msg;
    while (1) {
        if (xQueueReceive(g_audio_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {

            /* ---- Playback (blocking — prevents concurrent record) ---- */
            case AUDIO_CMD_PLAY_FEEDBACK: {
                payload_feedback_t *p = (payload_feedback_t *)msg.payload;
                if (p) {
                    audio_play_beep(p->correct ? 880 : 220,
                                    p->correct ? 200 : 400);
                } else {
                    audio_play_beep(523, 150);
                }
                break;
            }
            case AUDIO_CMD_PLAY_PROMPT: {
                /* M4: TF card WAV with fallback beep */
                prompt_id_t id = (prompt_id_t)(uintptr_t)msg.payload;
                audio_play_prompt(id);
                break;
            }
            case AUDIO_CMD_REPLAY_RECORDING: {
                audio_recording_t *rec = (audio_recording_t *)msg.payload;
                if (rec && rec->buffer && rec->sample_count > 0) {
                    audio_play_pcm(rec->buffer, rec->sample_count, rec->sample_rate);
                }
                audio_recording_free(rec);
                continue;  /* payload already freed above */
            }

            /* ---- Volume (always allowed, even during recording) ---- */
            case AUDIO_CMD_SET_VOLUME: {
                payload_volume_t *p = (payload_volume_t *)msg.payload;
                if (p) audio_set_volume(p->volume);
                break;
            }

            /* ---- Recording start ---- */
            case AUDIO_CMD_START_RECORDING: {
                if (audio_rec_is_active()) {
                    ESP_LOGW("AUDIO", "START_REC ignored: already recording");
                    break;
                }
                esp_err_t ret = audio_rec_start();
                if (ret != ESP_OK) {
                    /* PSRAM alloc failed — send error, back to IDLE */
                    app_msg_t err = {
                        .type = APP_EVT_AUDIO_ERROR,
                        .session_id = 0,
                        .error_code = ret,
                    };
                    queue_send_or_free(g_app_queue, &err);
                    break;
                }
                /* Recording loop: poll I2S RX + check stop/timeout/commands */
                audio_recording_t *rec = NULL;
                TickType_t start_tick = xTaskGetTickCount();
                while (audio_rec_is_active()) {
                    /* 1. Check 5 s timeout */
                    if ((xTaskGetTickCount() - start_tick)
                        >= pdMS_TO_TICKS(5000)) {
                        audio_rec_set_stop_reason(AUDIO_REC_STOP_TIMEOUT);
                        audio_rec_stop(&rec);
                        send_recording_done_or_free(rec);
                        break;
                    }

                    /* 2. Read I2S RX chunk */
                    uint32_t chunk = audio_rec_read_chunk();
                    if (chunk == 0 && !audio_rec_is_active()) {
                        /* Buffer full or error — auto-stopped */
                        audio_recording_t *auto_rec;
                        audio_rec_stop(&auto_rec);
                        send_recording_done_or_free(auto_rec);
                        break;
                    }

                    /* 3. Check for STOP / SET_VOLUME / PLAY_PROMPT (non-blocking) */
                    app_msg_t cmd;
                    if (xQueueReceive(g_audio_queue, &cmd, 0) == pdTRUE) {
                        switch (cmd.type) {
                        case AUDIO_CMD_STOP_RECORDING:
                            audio_rec_set_stop_reason(AUDIO_REC_STOP_USER);
                            audio_rec_stop(&rec);
                            send_recording_done_or_free(rec);
                            free(cmd.payload);
                            goto rec_done;
                        case AUDIO_CMD_SET_VOLUME: {
                            payload_volume_t *vp = (payload_volume_t *)cmd.payload;
                            if (vp) audio_set_volume(vp->volume);
                            free(cmd.payload);
                            break;
                        }
                        case AUDIO_CMD_PLAY_PROMPT:
                        case AUDIO_CMD_PLAY_FEEDBACK:
                        case AUDIO_CMD_REPLAY_RECORDING: {
                            /* Deny playback during recording */
                            app_msg_t err = {
                                .type = APP_EVT_AUDIO_ERROR,
                                .session_id = 0,
                                .error_code = ESP_ERR_INVALID_STATE,
                            };
                            queue_send_or_free(g_app_queue, &err);
                            free(cmd.payload);
                            break;
                        }
                        default:
                            free(cmd.payload);
                            break;
                        }
                    }
                }
            rec_done:
                break;
            }

            default:
                break;
            }
            free(msg.payload);
        }
    }
}

void ai_task_main(void *arg)
{
    printf("[AI   ] AI task ready, waiting on queue\n");
    app_msg_t msg;
    while (1) {
        if (xQueueReceive(g_ai_queue, &msg, portMAX_DELAY) == pdTRUE) {
            printf("[AI   ] M5: AI inference stub\n");
            free(msg.payload);
        }
    }
}

void diag_task_main(void *arg)
{
    esp_task_wdt_add(NULL);

    printf("[DIAG ] === Service Console Ready ===\n");
    printf("[DIAG ] Type 'help' for available commands.\n> ");
    fflush(stdout);

    char line[128];
    while (1) {
        esp_task_wdt_reset();

        if (fgets(line, sizeof(line), stdin)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] != '\0') {
                diag_console_execute(line);
                printf("> ");
                fflush(stdout);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(1000));  /* stdin not available — back off */
        }
    }
}
