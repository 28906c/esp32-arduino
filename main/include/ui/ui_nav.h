#ifndef UI_NAV_H
#define UI_NAV_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCREEN_PATIENT_SELECT,
    SCREEN_HOME,
    SCREEN_SETTINGS,
    SCREEN_LISTEN_TRAINING,
    SCREEN_NAMING_TRAINING,
    SCREEN_SENTENCE_BUILDING,
    SCREEN_PRONUNCIATION_TRAINING,
    SCREEN_TRAINING_RESULT,
    SCREEN_HISTORY,
} screen_id_t;

/* 初始化导航系统，par = 屏幕父对象（通常为 lv_scr_act()） */
void ui_nav_init(lv_obj_t *par);

/* 推入新页面（自动删除当前页面根容器） */
void ui_nav_push(screen_id_t screen);

/* 返回上一页 */
void ui_nav_back(void);

/* 获取当前页面根容器 */
lv_obj_t *ui_nav_get_root(void);

/* 获取当前 screen id */
screen_id_t ui_nav_current(void);

#ifdef __cplusplus
}
#endif

#endif
