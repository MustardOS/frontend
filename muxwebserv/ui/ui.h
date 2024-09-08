#pragma once

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlShell;
extern lv_obj_t *ui_pnlBrowser;
extern lv_obj_t *ui_pnlTerminal;
extern lv_obj_t *ui_pnlSyncthing;
extern lv_obj_t *ui_pnlResilio;
extern lv_obj_t *ui_pnlNTP;
extern lv_obj_t *ui_lblShell;
extern lv_obj_t *ui_lblBrowser;
extern lv_obj_t *ui_lblTerminal;
extern lv_obj_t *ui_lblSyncthing;
extern lv_obj_t *ui_lblResilio;
extern lv_obj_t *ui_lblNTP;
extern lv_obj_t *ui_icoShell;
extern lv_obj_t *ui_icoBrowser;
extern lv_obj_t *ui_icoTerminal;
extern lv_obj_t *ui_icoSyncthing;
extern lv_obj_t *ui_icoResilio;
extern lv_obj_t *ui_icoNTP;
extern lv_obj_t *ui_droShell;
extern lv_obj_t *ui_droBrowser;
extern lv_obj_t *ui_droTerminal;
extern lv_obj_t *ui_droSyncthing;
extern lv_obj_t *ui_droResilio;
extern lv_obj_t *ui_droNTP;

void ui_init(lv_obj_t *ui_pnlContent);
