#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlVersion;
lv_obj_t *ui_pnlDevice;
lv_obj_t *ui_pnlKernel;
lv_obj_t *ui_pnlUptime;
lv_obj_t *ui_pnlCPU;
lv_obj_t *ui_pnlSpeed;
lv_obj_t *ui_pnlGovernor;
lv_obj_t *ui_pnlMemory;
lv_obj_t *ui_pnlTemp;
lv_obj_t *ui_pnlServices;
lv_obj_t *ui_pnlBatteryCap;
lv_obj_t *ui_pnlVoltage;
lv_obj_t *ui_lblVersion;
lv_obj_t *ui_lblDevice;
lv_obj_t *ui_lblKernel;
lv_obj_t *ui_lblUptime;
lv_obj_t *ui_lblCPU;
lv_obj_t *ui_lblSpeed;
lv_obj_t *ui_lblGovernor;
lv_obj_t *ui_lblMemory;
lv_obj_t *ui_lblTemp;
lv_obj_t *ui_lblServices;
lv_obj_t *ui_lblBatteryCap;
lv_obj_t *ui_lblVoltage;
lv_obj_t *ui_icoVersion;
lv_obj_t *ui_icoDevice;
lv_obj_t *ui_icoKernel;
lv_obj_t *ui_icoUptime;
lv_obj_t *ui_icoCPU;
lv_obj_t *ui_icoSpeed;
lv_obj_t *ui_icoGovernor;
lv_obj_t *ui_icoMemory;
lv_obj_t *ui_icoTemp;
lv_obj_t *ui_icoServices;
lv_obj_t *ui_icoBatteryCap;
lv_obj_t *ui_icoVoltage;
lv_obj_t *ui_lblVersionValue;
lv_obj_t *ui_lblDeviceValue;
lv_obj_t *ui_lblKernelValue;
lv_obj_t *ui_lblUptimeValue;
lv_obj_t *ui_lblCPUValue;
lv_obj_t *ui_lblSpeedValue;
lv_obj_t *ui_lblGovernorValue;
lv_obj_t *ui_lblMemoryValue;
lv_obj_t *ui_lblTempValue;
lv_obj_t *ui_lblServicesValue;
lv_obj_t *ui_lblBatteryCapValue;
lv_obj_t *ui_lblVoltageValue;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
