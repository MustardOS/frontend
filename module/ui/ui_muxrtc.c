#include "ui_muxrtc.h"

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
    ui_pnlYear = lv_obj_create(ui_pnlContent);
    ui_pnlMonth = lv_obj_create(ui_pnlContent);
    ui_pnlDay = lv_obj_create(ui_pnlContent);
    ui_pnlHour = lv_obj_create(ui_pnlContent);
    ui_pnlMinute = lv_obj_create(ui_pnlContent);
    ui_pnlNotation = lv_obj_create(ui_pnlContent);
    ui_pnlTimezone = lv_obj_create(ui_pnlContent);

    ui_lblYear = lv_label_create(ui_pnlYear);
    ui_lblMonth = lv_label_create(ui_pnlMonth);
    ui_lblDay = lv_label_create(ui_pnlDay);
    ui_lblHour = lv_label_create(ui_pnlHour);
    ui_lblMinute = lv_label_create(ui_pnlMinute);
    ui_lblNotation = lv_label_create(ui_pnlNotation);
    ui_lblTimezone = lv_label_create(ui_pnlTimezone);

    ui_icoYear = lv_img_create(ui_pnlYear);
    ui_icoMonth = lv_img_create(ui_pnlMonth);
    ui_icoDay = lv_img_create(ui_pnlDay);
    ui_icoHour = lv_img_create(ui_pnlHour);
    ui_icoMinute = lv_img_create(ui_pnlMinute);
    ui_icoNotation = lv_img_create(ui_pnlNotation);
    ui_icoTimezone = lv_img_create(ui_pnlTimezone);

    ui_lblYearValue = lv_label_create(ui_pnlYear);
    ui_lblMonthValue = lv_label_create(ui_pnlMonth);
    ui_lblDayValue = lv_label_create(ui_pnlDay);
    ui_lblHourValue = lv_label_create(ui_pnlHour);
    ui_lblMinuteValue = lv_label_create(ui_pnlMinute);
    ui_lblNotationValue = lv_label_create(ui_pnlNotation);
    ui_lblTimezoneValue = lv_label_create(ui_pnlTimezone);
}
