#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 顶部标题栏：蓝色背景 + 标题 + 可选返回按钮 */
lv_obj_t *ui_top_bar_create(lv_obj_t *parent, const char *title, bool show_back,
                            lv_event_cb_t back_cb);

/* 大按钮 (64px 高，主色圆角) */
lv_obj_t *ui_big_button_create(lv_obj_t *parent, const char *text,
                               lv_coord_t w, lv_coord_t h,
                               lv_color_t color, lv_event_cb_t cb);

/* 白色卡片容器 */
lv_obj_t *ui_card_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/* Toast 提示：短暂显示后自动消失 */
void ui_toast_show(const char *text);

#ifdef __cplusplus
}
#endif

#endif
