#include "ui/ui_components.h"
#include "ui/ui_theme.h"
#include "ui/ui_font.h"

/* ---- Top Bar ---- */
lv_obj_t *ui_top_bar_create(lv_obj_t *parent, const char *title, bool show_back,
                            lv_event_cb_t back_cb)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, lv_pct(100), 50);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, TH_PRIMARY, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    if (show_back) {
        lv_obj_t *btn = lv_btn_create(bar);
        lv_obj_set_size(btn, 40, 40);
        lv_obj_align(btn, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_style_radius(btn, 20, 0);
        lv_obj_set_style_bg_color(btn, TH_PRIMARY_DARK, 0);
        lv_obj_set_style_border_width(btn, 0, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, "<");
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, TH_TEXT_ON_PRIMARY, 0);
        lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);

        if (back_cb) {
            lv_obj_add_event_cb(btn, back_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    lv_obj_t *tl = lv_label_create(bar);
    lv_label_set_text(tl, title);
    lv_obj_center(tl);
    lv_obj_set_style_text_color(tl, TH_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(tl, ui_font_get(TH_FONT_MD), 0);

    return bar;
}

/* ---- Big Button ---- */
lv_obj_t *ui_big_button_create(lv_obj_t *parent, const char *text,
                               lv_coord_t w, lv_coord_t h,
                               lv_color_t color, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, w > 0 ? w : lv_pct(80));
    lv_obj_set_height(btn, h > 0 ? h : TH_BTN_H_LG);
    lv_obj_set_style_radius(btn, TH_BTN_RADIUS, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, TH_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);

    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }
    return btn;
}

/* ---- Card ---- */
lv_obj_t *ui_card_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    th_style_card(card);
    return card;
}

/* ---- Toast ---- */
static void toast_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *overlay = (lv_obj_t *)timer->user_data;
    if (overlay && lv_obj_is_valid(overlay)) {
        lv_obj_del(overlay);
    }
    /* timer auto-deleted by LVGL (repeat_count set to 1 in ui_toast_show) */
}

void ui_toast_show(const char *text)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *toast = lv_obj_create(scr);
    lv_obj_set_style_bg_color(toast, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_90, 0);
    lv_obj_set_style_radius(toast, 8, 0);
    lv_obj_set_style_border_width(toast, 0, 0);

    lv_obj_t *lbl = lv_label_create(toast);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, ui_font_get(TH_FONT_MD), 0);

    lv_obj_set_size(toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(toast);
    lv_obj_update_layout(toast);
    lv_coord_t w = lv_obj_get_width(toast);
    lv_coord_t h = lv_obj_get_height(toast);
    lv_obj_set_size(toast, w + TH_SPACING_XL, h + TH_SPACING_MD);

    lv_obj_move_foreground(toast);
    lv_timer_t *timer = lv_timer_create(toast_timer_cb, 2000, toast);
    lv_timer_set_repeat_count(timer, 1);
}
