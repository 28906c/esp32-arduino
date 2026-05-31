#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ---- 内置命令处理 ---- */

static int cmd_help(void)
{
    printf("\n");
    printf("  help          Show this help\n");
    printf("  status        System status overview\n");
    printf("  heap          Heap memory info\n");
    printf("  tasks         Task list with stack HWM\n");
    printf("\n");
    return 0;
}

static int cmd_status(void)
{
    size_t psram_size = esp_psram_get_size();
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);

    printf("\n");
    printf("  === System Status ===\n");
    printf("  Chip:        ESP32-P4\n");
    printf("  Free heap:   %lu bytes\n", (unsigned long)free_heap);
    printf("  Min heap:    %lu bytes\n", (unsigned long)min_free_heap);
    printf("  PSRAM size:  %u bytes\n", (unsigned)psram_size);
    printf("  Task count:  %u\n", (unsigned)uxTaskGetNumberOfTasks());
    printf("\n");
    return 0;
}

static int cmd_heap(void)
{
    printf("\n");
    printf("  === Heap Info ===\n");
    printf("  DRAM free:       %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf("  DRAM min free:   %u\n", (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    printf("  DRAM largest:    %u\n", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("  PSRAM free:      %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("  PSRAM largest:   %u\n", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    printf("\n");
    return 0;
}

static int cmd_tasks(void)
{
    char buf[1024] = {0};
    vTaskList(buf);
    buf[sizeof(buf) - 1] = '\0';  /* vTaskList may not null-terminate on overflow */
    printf("\n");
    printf("  === Task List ===\n");
    printf("  %s", buf);
    return 0;
}

/* ---- 命令分发 ---- */

int diag_console_execute(const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    if (strcmp(line, "help") == 0) {
        return cmd_help();
    } else if (strcmp(line, "status") == 0) {
        return cmd_status();
    } else if (strcmp(line, "heap") == 0) {
        return cmd_heap();
    } else if (strcmp(line, "tasks") == 0) {
        return cmd_tasks();
    } else {
        printf("  Unknown command: '%s'. Type 'help' for available commands.\n", line);
        return -1;
    }
}
