#pragma once

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlVersion;
extern lv_obj_t *ui_pnlDevice;
extern lv_obj_t *ui_pnlKernel;
extern lv_obj_t *ui_pnlUptime;
extern lv_obj_t *ui_pnlCPU;
extern lv_obj_t *ui_pnlSpeed;
extern lv_obj_t *ui_pnlGovernor;
extern lv_obj_t *ui_pnlMemory;
extern lv_obj_t *ui_pnlTemp;
extern lv_obj_t *ui_pnlServices;
extern lv_obj_t *ui_pnlBatteryCap;
extern lv_obj_t *ui_pnlVoltage;
extern lv_obj_t *ui_lblVersion;
extern lv_obj_t *ui_lblDevice;
extern lv_obj_t *ui_lblKernel;
extern lv_obj_t *ui_lblUptime;
extern lv_obj_t *ui_lblCPU;
extern lv_obj_t *ui_lblSpeed;
extern lv_obj_t *ui_lblGovernor;
extern lv_obj_t *ui_lblMemory;
extern lv_obj_t *ui_lblTemp;
extern lv_obj_t *ui_lblServices;
extern lv_obj_t *ui_lblBatteryCap;
extern lv_obj_t *ui_lblVoltage;
extern lv_obj_t *ui_icoVersion;
extern lv_obj_t *ui_icoDevice;
extern lv_obj_t *ui_icoKernel;
extern lv_obj_t *ui_icoUptime;
extern lv_obj_t *ui_icoCPU;
extern lv_obj_t *ui_icoSpeed;
extern lv_obj_t *ui_icoGovernor;
extern lv_obj_t *ui_icoMemory;
extern lv_obj_t *ui_icoTemp;
extern lv_obj_t *ui_icoServices;
extern lv_obj_t *ui_icoBatteryCap;
extern lv_obj_t *ui_icoVoltage;
extern lv_obj_t *ui_lblVersionValue;
extern lv_obj_t *ui_lblDeviceValue;
extern lv_obj_t *ui_lblKernelValue;
extern lv_obj_t *ui_lblUptimeValue;
extern lv_obj_t *ui_lblCPUValue;
extern lv_obj_t *ui_lblSpeedValue;
extern lv_obj_t *ui_lblGovernorValue;
extern lv_obj_t *ui_lblMemoryValue;
extern lv_obj_t *ui_lblTempValue;
extern lv_obj_t *ui_lblServicesValue;
extern lv_obj_t *ui_lblBatteryCapValue;
extern lv_obj_t *ui_lblVoltageValue;

void ui_init(lv_obj_t *ui_pnlContent);
