idf_component_register(
    SRCS
        "app_main.c"
        "src/app/app_tasks.c"
        "src/app/app_controller.c"
        "src/app/training_controller.c"

        "src/diagnostics/console.c"

        "src/ui/ui_app.c"
        "src/ui/ui_font.c"
        "src/ui/lv_font_ascii_24.c"
        "src/ui/lv_font_chinese_24.c"
        "src/ui/ui_nav.c"
        "src/ui/ui_components.c"
        "src/ui/ui_screens.c"
        "src/audio/audio_manager.c"
        "src/audio/audio_prompts.c"
        "src/storage/storage_task.c"
    INCLUDE_DIRS
        "include"
        "include/app"
        "include/platform"
        "include/ui"
        "include/training"
        "include/audio"
        "include/ai"
        "include/storage"
        "include/diagnostics"
    REQUIRES
        esp_psram
        nvs_flash
        lvgl
        esp_codec_dev
        fatfs
        sdmmc
)

# Force UTF-8 for source file encoding (Chinese Windows GCC workaround)
target_compile_options(__idf_main PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
