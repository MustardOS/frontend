#include "ui_muxspace.h"
#include "../../common/device.h"
#include "../../common/theme.h"

lv_obj_t *ui_pnlSD1;
lv_obj_t *ui_pnlSD2;
lv_obj_t *ui_pnlUSB;
lv_obj_t *ui_pnlRFS;

lv_obj_t *ui_lblSD1;
lv_obj_t *ui_lblSD2;
lv_obj_t *ui_lblUSB;
lv_obj_t *ui_lblRFS;

lv_obj_t *ui_icoSD1;
lv_obj_t *ui_icoSD2;
lv_obj_t *ui_icoUSB;
lv_obj_t *ui_icoRFS;

lv_obj_t *ui_lblSD1Value;
lv_obj_t *ui_lblSD2Value;
lv_obj_t *ui_lblUSBValue;
lv_obj_t *ui_lblRFSValue;

lv_obj_t *ui_pnlSD1Bar;
lv_obj_t *ui_pnlSD2Bar;
lv_obj_t *ui_pnlUSBBar;
lv_obj_t *ui_pnlRFSBar;

lv_obj_t *ui_barSD1;
lv_obj_t *ui_barSD2;
lv_obj_t *ui_barUSB;
lv_obj_t *ui_barRFS;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlSD1 = lv_obj_create(ui_pnlContent);
    ui_pnlSD1Bar = lv_obj_create(ui_pnlContent);
    ui_pnlSD2 = lv_obj_create(ui_pnlContent);
    ui_pnlSD2Bar = lv_obj_create(ui_pnlContent);
    ui_pnlUSB = lv_obj_create(ui_pnlContent);
    ui_pnlUSBBar = lv_obj_create(ui_pnlContent);
    ui_pnlRFS = lv_obj_create(ui_pnlContent);
    ui_pnlRFSBar = lv_obj_create(ui_pnlContent);

    ui_lblSD1 = lv_label_create(ui_pnlSD1);
    ui_lblSD2 = lv_label_create(ui_pnlSD2);
    ui_lblUSB = lv_label_create(ui_pnlUSB);
    ui_lblRFS = lv_label_create(ui_pnlRFS);

    ui_icoSD1 = lv_img_create(ui_pnlSD1);
    ui_icoSD2 = lv_img_create(ui_pnlSD2);
    ui_icoUSB = lv_img_create(ui_pnlUSB);
    ui_icoRFS = lv_img_create(ui_pnlRFS);

    ui_lblSD1Value = lv_label_create(ui_pnlSD1);
    ui_lblSD2Value = lv_label_create(ui_pnlSD2);
    ui_lblUSBValue = lv_label_create(ui_pnlUSB);
    ui_lblRFSValue = lv_label_create(ui_pnlRFS);

    ui_barSD1 = lv_bar_create(ui_pnlSD1Bar);
    ui_barSD2 = lv_bar_create(ui_pnlSD2Bar);
    ui_barUSB = lv_bar_create(ui_pnlUSBBar);
    ui_barRFS = lv_bar_create(ui_pnlRFSBar);

    lv_obj_set_height(ui_pnlSD1Bar, 10);
    lv_obj_set_width(ui_pnlSD1Bar, lv_pct(100));
    lv_obj_set_width(ui_barSD1, lv_pct(100));

    lv_obj_set_height(ui_pnlSD2Bar, 10);
    lv_obj_set_width(ui_pnlSD2Bar, device.MUX.WIDTH);
    lv_obj_set_width(ui_barSD2, device.MUX.WIDTH);

    lv_obj_set_height(ui_pnlUSBBar, 10);
    lv_obj_set_width(ui_pnlUSBBar, device.MUX.WIDTH);
    lv_obj_set_width(ui_barUSB, device.MUX.WIDTH);

    lv_obj_set_height(ui_pnlRFSBar, 10);
    lv_obj_set_width(ui_pnlRFSBar, lv_pct(100));
    lv_obj_set_width(ui_barRFS, lv_pct(100));

    lv_bar_set_range(ui_barSD1, 0, device.MUX.WIDTH);
    lv_obj_set_style_bg_color(ui_barSD1, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barSD1, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barSD1, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barSD1, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_bar_set_range(ui_barSD2, 0, 100);
    lv_obj_set_width(ui_barSD2, device.MUX.WIDTH);
    lv_obj_set_style_bg_color(ui_barSD2, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barSD2, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barSD2, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barSD2, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_bar_set_range(ui_barUSB, 0, 100);
    lv_obj_set_width(ui_barUSB, device.MUX.WIDTH);
    lv_obj_set_style_bg_color(ui_barUSB, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barUSB, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barUSB, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barUSB, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_bar_set_range(ui_barRFS, 0, 100);
    lv_obj_set_width(ui_barRFS, device.MUX.WIDTH);
    lv_obj_set_style_bg_color(ui_barRFS, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barRFS, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barRFS, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barRFS, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
