#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Task priorities (higher = preempts lower) ---- */
#define TASK_PRIO_UI        4
#define TASK_PRIO_APP       5
#define TASK_PRIO_AUDIO     6
#define TASK_PRIO_AI        5
#define TASK_PRIO_STORAGE   3
#define TASK_PRIO_DIAG      2

/* ---- Task stack sizes (bytes) ---- */
#define TASK_STACK_UI       65536
#define TASK_STACK_APP      6144
#define TASK_STACK_AUDIO    8192
#define TASK_STACK_AI       8192
#define TASK_STACK_STORAGE  6144
#define TASK_STACK_DIAG     4096

/* ---- Queue depths ---- */
#define QUEUE_LEN_UI        16
#define QUEUE_LEN_APP       16
#define QUEUE_LEN_AUDIO     8
#define QUEUE_LEN_AI        8
#define QUEUE_LEN_STORAGE   8
#define QUEUE_LEN_DIAG      8

/* ---- Global queue handles ---- */
extern QueueHandle_t g_ui_queue;
extern QueueHandle_t g_app_queue;
extern QueueHandle_t g_audio_queue;
extern QueueHandle_t g_ai_queue;
extern QueueHandle_t g_storage_queue;
extern QueueHandle_t g_diag_queue;

/* ---- Task entry points ---- */
void ui_task_main(void *arg);
void app_task_main(void *arg);
void audio_task_main(void *arg);
void ai_task_main(void *arg);
void storage_task_main(void *arg);
void diag_task_main(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASKS_H */
