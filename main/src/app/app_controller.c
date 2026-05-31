#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "app/app_tasks.h"
#include "app/app_events.h"
#include "app/training_controller.h"
#include "audio/audio_manager.h"

/*
 * App Controller — message dispatch loop.
 * Receives events from g_app_queue, routes them to the training
 * state machine or forwards to UI.  Payload ownership: caller
 * (this file) frees msg.payload after dispatch unless ownership
 * is transferred (e.g. audio_recording_t forwarded to UI).
 */

void app_task_main(void *arg)
{
    printf("[APP  ] App controller ready, waiting on queue\n");
    tc_init();
    app_msg_t msg;
    while (1) {
        if (xQueueReceive(g_app_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {

            /* ---- UI events → training controller ---- */
            case UI_EVT_MODULE_SELECTED: {
                payload_module_selected_t *p = (payload_module_selected_t *)msg.payload;
                if (p) tc_session_start(p->module);
                break;
            }
            case UI_EVT_ANSWER_SELECTED:
            case UI_EVT_RECORD_PRESSED:
            case UI_EVT_REPLAY_PRESSED:
            case UI_EVT_FEEDBACK_DONE:
            case UI_EVT_SENTENCE_CONFIRM:
                tc_handle_event(msg.type, msg.payload, msg.session_id);
                break;

            /* ---- Audio → UI: recording done ---- */
            case APP_EVT_RECORDING_DONE: {
                /* Forward recording ownership to UI for replay.
                 * UI must call audio_recording_free() when done. */
                queue_send_or_free(g_ui_queue, &msg);
                continue;  /* skip free(msg.payload) — ownership transferred */
            }

            /* ---- Audio error → UI toast ---- */
            case APP_EVT_AUDIO_ERROR: {
                /* Forward error to UI */
                queue_send_or_free(g_ui_queue, &msg);
                continue;
            }

            default:
                break;
            }
            free(msg.payload);
        }
    }
}
