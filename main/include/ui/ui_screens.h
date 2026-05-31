#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "ui/ui_nav.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建患者选择页 */
void ui_create_patient_select(lv_obj_t *root);

/* 创建首页 */
void ui_create_home(lv_obj_t *root);

/* 创建设置页 */
void ui_create_settings(lv_obj_t *root);

/* M3: 听理解训练页 */
void ui_create_listen_training(lv_obj_t *root);

/* M3: 命名训练页 */
void ui_create_naming_training(lv_obj_t *root);

/* M3: 训练结果页 */
void ui_create_training_result(lv_obj_t *root);
void ui_create_sentence_building(lv_obj_t *root);
void ui_create_pronunciation_training(lv_obj_t *root);
void ui_create_history(lv_obj_t *root);

/* M3: in-place update functions (called from UI task message dispatch) */
void ui_listen_show_feedback(void *fb_payload);
void ui_listen_update_question(uint8_t q_idx);
void ui_naming_update_question(uint8_t q_idx);
void ui_naming_show_ready(void *payload);
void ui_sentence_update_question(uint8_t q_idx);
void ui_set_result_data(uint8_t correct, uint8_t total, uint32_t elapsed, int module);
screen_id_t ui_get_pending_module(void);

#ifdef __cplusplus
}
#endif

#endif
