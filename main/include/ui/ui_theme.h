#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 色板 (REBUILD spec) ===== */
#define TH_PRIMARY           lv_color_hex(0x1976D2)
#define TH_PRIMARY_BG        lv_color_hex(0xE3F2FD)
#define TH_PRIMARY_DARK      lv_color_hex(0x1565C0)
#define TH_SUCCESS           lv_color_hex(0x2E7D32)
#define TH_WARNING           lv_color_hex(0xF9A825)
#define TH_ERROR             lv_color_hex(0xC62828)
#define TH_DANGER            lv_color_hex(0xC62828)
#define TH_BG                lv_color_hex(0xF6F7F9)
#define TH_CARD_BG           lv_color_hex(0xFFFFFF)
#define TH_TEXT              lv_color_hex(0x1F2933)
#define TH_TEXT_SECONDARY    lv_color_hex(0x6B7280)
#define TH_TEXT_ON_PRIMARY   lv_color_hex(0xFFFFFF)

#define TH_MODULE_LISTEN     lv_color_hex(0x42A5F5)
#define TH_MODULE_SENTENCE   lv_color_hex(0x66BB6A)
#define TH_MODULE_NAMING     lv_color_hex(0xFFA726)
#define TH_MODULE_PRON       lv_color_hex(0xEF5350)

/* ===== 尺寸 ===== */
#define TH_BTN_H_SM          44
#define TH_BTN_H_MD          56
#define TH_BTN_H_LG          64
#define TH_BTN_RADIUS        12
#define TH_CARD_RADIUS       12
#define TH_SPACING_SM        8
#define TH_SPACING_MD        12
#define TH_SPACING_LG        16
#define TH_SPACING_XL        24

#define TH_FONT_SM           20
#define TH_FONT_MD           24
#define TH_FONT_LG           32
#define TH_FONT_XL           48

/* ===== 快捷样式 ===== */
static inline void th_style_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, TH_CARD_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, TH_CARD_RADIUS, 0);
    lv_obj_set_style_shadow_width(obj, 4, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 2, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

static inline void th_style_primary_btn(lv_obj_t *btn)
{
    lv_obj_set_style_radius(btn, TH_BTN_RADIUS, 0);
    lv_obj_set_style_bg_color(btn, TH_PRIMARY, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_bg_color(btn, TH_PRIMARY_DARK, LV_STATE_PRESSED);
}

#ifdef __cplusplus
}
#endif

#endif
