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

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlYear = lv_obj_create(ui_pnlContent);
    ui_pnlMonth = lv_obj_create(ui_pnlContent);
    ui_pnlDay = lv_obj_create(ui_pnlContent);
    ui_pnlHour = lv_obj_create(ui_pnlContent);
    ui_pnlMinute = lv_obj_create(ui_pnlContent);
    ui_pnlNotation = lv_obj_create(ui_pnlContent);
    ui_pnlTimezone = lv_obj_create(ui_pnlContent);

    ui_lblYear = lv_label_create(ui_pnlYear);
    lv_label_set_text(ui_lblYear, "");
    ui_lblMonth = lv_label_create(ui_pnlMonth);
    lv_label_set_text(ui_lblMonth, "");
    ui_lblDay = lv_label_create(ui_pnlDay);
    lv_label_set_text(ui_lblDay, "");
    ui_lblHour = lv_label_create(ui_pnlHour);
    lv_label_set_text(ui_lblHour, "");
    ui_lblMinute = lv_label_create(ui_pnlMinute);
    lv_label_set_text(ui_lblMinute, "");
    ui_lblNotation = lv_label_create(ui_pnlNotation);
    lv_label_set_text(ui_lblNotation, "");
    ui_lblTimezone = lv_label_create(ui_pnlTimezone);
    lv_label_set_text(ui_lblTimezone, "");

    ui_icoYear = lv_img_create(ui_pnlYear);
    ui_icoMonth = lv_img_create(ui_pnlMonth);
    ui_icoDay = lv_img_create(ui_pnlDay);
    ui_icoHour = lv_img_create(ui_pnlHour);
    ui_icoMinute = lv_img_create(ui_pnlMinute);
    ui_icoNotation = lv_img_create(ui_pnlNotation);
    ui_icoTimezone = lv_img_create(ui_pnlTimezone);

    ui_lblYearValue = lv_label_create(ui_pnlYear);
    lv_label_set_text(ui_lblYearValue, "");
    ui_lblMonthValue = lv_label_create(ui_pnlMonth);
    lv_label_set_text(ui_lblMonthValue, "");
    ui_lblDayValue = lv_label_create(ui_pnlDay);
    lv_label_set_text(ui_lblDayValue, "");
    ui_lblHourValue = lv_label_create(ui_pnlHour);
    lv_label_set_text(ui_lblHourValue, "");
    ui_lblMinuteValue = lv_label_create(ui_pnlMinute);
    lv_label_set_text(ui_lblMinuteValue, "");
    ui_lblNotationValue = lv_label_create(ui_pnlNotation);
    lv_label_set_text(ui_lblNotationValue, "");
    ui_lblTimezoneValue = lv_label_create(ui_pnlTimezone);
    lv_label_set_text(ui_lblTimezoneValue, "");
}
