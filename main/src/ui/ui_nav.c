#include <string.h>
#include "ui/ui_nav.h"
#include "ui/ui_screens.h"

#define NAV_MAX_DEPTH 8

static lv_obj_t    *s_parent = NULL;
static lv_obj_t    *s_root   = NULL;
static screen_id_t  s_stack[NAV_MAX_DEPTH];
static int          s_depth  = 0;

void ui_nav_init(lv_obj_t *par)
{
    s_parent = par;
    s_depth  = 0;
    memset(s_stack, 0, sizeof(s_stack));
}

static void delete_root(void)
{
    if (s_root && lv_obj_is_valid(s_root)) {
        lv_obj_del(s_root);
    }
    s_root = NULL;
}

static lv_obj_t *create_root(void)
{
    s_root = lv_obj_create(s_parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0xF6F7F9), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    return s_root;
}

static void screen_create(screen_id_t screen, lv_obj_t *root)
{
    switch (screen) {
    case SCREEN_PATIENT_SELECT:          ui_create_patient_select(root);          break;
    case SCREEN_HOME:                    ui_create_home(root);                    break;
    case SCREEN_SETTINGS:                ui_create_settings(root);                break;
    case SCREEN_LISTEN_TRAINING:         ui_create_listen_training(root);         break;
    case SCREEN_NAMING_TRAINING:         ui_create_naming_training(root);         break;
    case SCREEN_SENTENCE_BUILDING:       ui_create_sentence_building(root);       break;
    case SCREEN_PRONUNCIATION_TRAINING:  ui_create_pronunciation_training(root);  break;
    case SCREEN_TRAINING_RESULT:         ui_create_training_result(root);         break;
    case SCREEN_HISTORY:                 ui_create_history(root);                 break;
    }
}

void ui_nav_push(screen_id_t screen)
{
    if (s_depth >= NAV_MAX_DEPTH) return;

    delete_root();
    s_stack[s_depth++] = screen;
    screen_create(screen, create_root());
}

void ui_nav_back(void)
{
    if (s_depth <= 1) return;

    s_depth--;
    delete_root();
    screen_create(s_stack[s_depth - 1], create_root());
}

lv_obj_t *ui_nav_get_root(void)
{
    return s_root;
}

screen_id_t ui_nav_current(void)
{
    if (s_depth > 0) return s_stack[s_depth - 1];
    return SCREEN_PATIENT_SELECT;
}
