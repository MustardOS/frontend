#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlVersion = lv_obj_create(ui_pnlContent);
    ui_pnlDevice = lv_obj_create(ui_pnlContent);
    ui_pnlKernel = lv_obj_create(ui_pnlContent);
    ui_pnlUptime = lv_obj_create(ui_pnlContent);
    ui_pnlCPU = lv_obj_create(ui_pnlContent);
    ui_pnlSpeed = lv_obj_create(ui_pnlContent);
    ui_pnlGovernor = lv_obj_create(ui_pnlContent);
    ui_pnlMemory = lv_obj_create(ui_pnlContent);
    ui_pnlTemp = lv_obj_create(ui_pnlContent);
    ui_pnlServices = lv_obj_create(ui_pnlContent);
    ui_pnlBatteryCap = lv_obj_create(ui_pnlContent);
    ui_pnlVoltage = lv_obj_create(ui_pnlContent);

    ui_lblVersion = lv_label_create(ui_pnlVersion);
    ui_lblDevice = lv_label_create(ui_pnlDevice);
    ui_lblKernel = lv_label_create(ui_pnlKernel);
    ui_lblUptime = lv_label_create(ui_pnlUptime);
    ui_lblCPU = lv_label_create(ui_pnlCPU);
    ui_lblSpeed = lv_label_create(ui_pnlSpeed);
    ui_lblGovernor = lv_label_create(ui_pnlGovernor);
    ui_lblMemory = lv_label_create(ui_pnlMemory);
    ui_lblTemp = lv_label_create(ui_pnlTemp);
    ui_lblServices = lv_label_create(ui_pnlServices);
    ui_lblBatteryCap = lv_label_create(ui_pnlBatteryCap);
    ui_lblVoltage = lv_label_create(ui_pnlVoltage);

    ui_icoVersion = lv_img_create(ui_pnlVersion);
    ui_icoDevice = lv_img_create(ui_pnlDevice);
    ui_icoKernel = lv_img_create(ui_pnlKernel);
    ui_icoUptime = lv_img_create(ui_pnlUptime);
    ui_icoCPU = lv_img_create(ui_pnlCPU);
    ui_icoSpeed = lv_img_create(ui_pnlSpeed);
    ui_icoGovernor = lv_img_create(ui_pnlGovernor);
    ui_icoMemory = lv_img_create(ui_pnlMemory);
    ui_icoTemp = lv_img_create(ui_pnlTemp);
    ui_icoServices = lv_img_create(ui_pnlServices);
    ui_icoBatteryCap = lv_img_create(ui_pnlBatteryCap);
    ui_icoVoltage = lv_img_create(ui_pnlVoltage);

    ui_lblVersionValue = lv_label_create(ui_pnlVersion);
    ui_lblDeviceValue = lv_label_create(ui_pnlDevice);
    ui_lblKernelValue = lv_label_create(ui_pnlKernel);
    ui_lblUptimeValue = lv_label_create(ui_pnlUptime);
    ui_lblCPUValue = lv_label_create(ui_pnlCPU);
    ui_lblSpeedValue = lv_label_create(ui_pnlSpeed);
    ui_lblGovernorValue = lv_label_create(ui_pnlGovernor);
    ui_lblMemoryValue = lv_label_create(ui_pnlMemory);
    ui_lblTempValue = lv_label_create(ui_pnlTemp);
    ui_lblServicesValue = lv_label_create(ui_pnlServices);
    ui_lblBatteryCapValue = lv_label_create(ui_pnlBatteryCap);
    ui_lblVoltageValue = lv_label_create(ui_pnlVoltage);
}
