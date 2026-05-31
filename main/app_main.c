#include <stdio.h>
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app/app_tasks.h"
#include "app/app_events.h"
#include "diagnostics/diag_console.h"

/* ---- Global queue handles ---- */
QueueHandle_t g_ui_queue;
QueueHandle_t g_app_queue;
QueueHandle_t g_audio_queue;
QueueHandle_t g_ai_queue;
QueueHandle_t g_storage_queue;
QueueHandle_t g_diag_queue;

static void print_banner(void)
{
    size_t psram_size = esp_psram_get_size();
    uint32_t free_heap = esp_get_free_heap_size();

    printf("\n");
    printf("[BOOT] ========================================\n");
    printf("[BOOT]   VOCA - Voice Companion for Aphasia v2.0.0\n");
    printf("[BOOT]   Target: ESP32-P4 | IDF: v5.4.3\n");
    printf("[BOOT]   PSRAM: Hex 200MHz | Flash: 16MB\n");
    printf("[BOOT] ========================================\n");
    printf("[BOOT] Free heap:  %lu bytes\n", (unsigned long)free_heap);
    printf("[BOOT] PSRAM size: %u bytes\n", (unsigned)psram_size);
    printf("[BOOT] Creating message queues...\n");
}

static void create_queues(void)
{
    g_ui_queue      = xQueueCreate(QUEUE_LEN_UI,      sizeof(app_msg_t));
    g_app_queue     = xQueueCreate(QUEUE_LEN_APP,     sizeof(app_msg_t));
    g_audio_queue   = xQueueCreate(QUEUE_LEN_AUDIO,   sizeof(app_msg_t));
    g_ai_queue      = xQueueCreate(QUEUE_LEN_AI,      sizeof(app_msg_t));
    g_storage_queue = xQueueCreate(QUEUE_LEN_STORAGE, sizeof(app_msg_t));
    g_diag_queue    = xQueueCreate(QUEUE_LEN_DIAG,    sizeof(app_msg_t));

    configASSERT(g_ui_queue && g_app_queue && g_audio_queue
              && g_ai_queue && g_storage_queue && g_diag_queue);
}

static void create_tasks(void)
{
    printf("[BOOT] Creating tasks...\n");

    xTaskCreatePinnedToCore(ui_task_main,      "ui",      TASK_STACK_UI,      NULL, TASK_PRIO_UI,      NULL, 0);
    xTaskCreate(app_task_main,     "app",     TASK_STACK_APP,     NULL, TASK_PRIO_APP,     NULL);
    xTaskCreate(audio_task_main,   "audio",   TASK_STACK_AUDIO,   NULL, TASK_PRIO_AUDIO,   NULL);
    xTaskCreate(ai_task_main,      "ai",      TASK_STACK_AI,      NULL, TASK_PRIO_AI,      NULL);
    xTaskCreate(storage_task_main, "storage", TASK_STACK_STORAGE, NULL, TASK_PRIO_STORAGE, NULL);
    xTaskCreate(diag_task_main,    "diag",    TASK_STACK_DIAG,    NULL, TASK_PRIO_DIAG,    NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    print_banner();
    create_queues();
    create_tasks();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
