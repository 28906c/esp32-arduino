#ifndef UI_FONT_H
#define UI_FONT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 根据逻辑字号返回字体指针。
 * M2 只生成 24px 中文，所有 size 暂时返回同一字体。
 * M3+ 生成多尺寸后按 size 分支。 */
const lv_font_t *ui_font_get(int size);

#ifdef __cplusplus
}
#endif

#endif
