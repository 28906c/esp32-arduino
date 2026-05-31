#include "ui/ui_font.h"
#include "ui/lv_font_chinese_24.h"

/*
 * Font size mapping:
 *   We only have one Chinese-capable font (24px). Built-in Montserrat
 *   fonts are used for ASCII-only labels at larger sizes to create
 *   visual hierarchy (e.g. "5/5" score on result page).
 *
 *   To add more Chinese font sizes: generate 20px / 32px / 48px bitmap
 *   fonts with LVGL font converter, then branch them in the switch below.
 */
const lv_font_t *ui_font_get(int size)
{
    switch (size) {
    case 48:
        return &lv_font_montserrat_48;
    case 32:
        return &lv_font_montserrat_32;
    case 24:
    case 20:
    default:
        return &lv_font_chinese_24;
    }
}
