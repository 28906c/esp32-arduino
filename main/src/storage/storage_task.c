#include "storage/storage.h"
#include "app/app_tasks.h"
#include "app/app_events.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <sys/stat.h>

static const char *TAG = "STORE";

static bool s_ready = false;

bool storage_is_ready(void)
{
    return s_ready;
}

static void create_dirs(void)
{
    mkdir("/sdcard/voca", 0755);
    mkdir("/sdcard/voca/prompts", 0755);
    mkdir("/sdcard/voca/recordings", 0755);
    mkdir("/sdcard/voca/models", 0755);
    mkdir("/sdcard/voca/images", 0755);
}

static bool mount_tf_card(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;  /* ESP32-P4-Nano uses 1-bit SDMMC */

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_card_t *card = NULL;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host,
                                            &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TF card mount failed: %s (continuing without storage)",
                 esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "TF card mounted: %s, capacity=%llu MB",
             card->cid.name, (unsigned long long)(card->csd.capacity * 512 / (1024 * 1024)));
    return true;
}

void storage_task_main(void *arg)
{
    ESP_LOGI(TAG, "Storage task starting, mounting TF card...");

    if (mount_tf_card()) {
        create_dirs();
        s_ready = true;
        ESP_LOGI(TAG, "Storage ready");
    }

    /* M7: extended with save/load operations via queue */

    app_msg_t msg;
    while (1) {
        if (xQueueReceive(g_storage_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "M7: storage op stub (type=0x%04lX)", (unsigned long)msg.type);
            free(msg.payload);
        }
    }
}
