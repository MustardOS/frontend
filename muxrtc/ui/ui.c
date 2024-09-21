#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlYear;
lv_obj_t *ui_pnlMonth;
lv_obj_t *ui_pnlDay;
lv_obj_t *ui_pnlHour;
lv_obj_t *ui_pnlMinute;
lv_obj_t *ui_pnlNotation;
lv_obj_t *ui_pnlTimezone;
lv_obj_t *ui_lblYear;
lv_obj_t *ui_lblMonth;
lv_obj_t *ui_lblDay;
lv_obj_t *ui_lblHour;
lv_obj_t *ui_lblMinute;
lv_obj_t *ui_lblNotation;
lv_obj_t *ui_lblTimezone;
lv_obj_t *ui_icoYear;
lv_obj_t *ui_icoMonth;
lv_obj_t *ui_icoDay;
lv_obj_t *ui_icoHour;
lv_obj_t *ui_icoMinute;
lv_obj_t *ui_icoNotation;
lv_obj_t *ui_icoTimezone;
lv_obj_t *ui_lblYearValue;
lv_obj_t *ui_lblMonthValue;
lv_obj_t *ui_lblDayValue;
lv_obj_t *ui_lblHourValue;
lv_obj_t *ui_lblMinuteValue;
lv_obj_t *ui_lblNotationValue;
lv_obj_t *ui_lblTimezoneValue;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
