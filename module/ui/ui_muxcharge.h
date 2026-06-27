#pragma once

#include "../../lvgl/lvgl.h"

void init_muxcharge(void);

extern lv_obj_t *ui_scr_charge_charge;
extern lv_obj_t *ui_blank_charge;

extern lv_obj_t *ui_img_wall_charge;

extern lv_obj_t *ui_pnl_wall_charge;
extern lv_obj_t *ui_pnl_charge_charge;

extern lv_obj_t *ui_lbl_capacity_charge;
extern lv_obj_t *ui_lbl_voltage_charge;
extern lv_obj_t *ui_lbl_boot_charge;
