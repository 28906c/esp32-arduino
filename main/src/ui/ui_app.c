#include "ui/ui_nav.h"
#include "ui/ui_screens.h"

void ui_app_start(void)
{
    ui_nav_init(lv_scr_act());
    ui_nav_push(SCREEN_PATIENT_SELECT);
}
