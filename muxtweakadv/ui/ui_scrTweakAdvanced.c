#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlAccelerate = lv_obj_create(ui_pnlContent);
    ui_pnlSwap = lv_obj_create(ui_pnlContent);
    ui_pnlThermal = lv_obj_create(ui_pnlContent);
    ui_pnlFont = lv_obj_create(ui_pnlContent);
    ui_pnlVolume = lv_obj_create(ui_pnlContent);
    ui_pnlBrightness = lv_obj_create(ui_pnlContent);
    ui_pnlOffset = lv_obj_create(ui_pnlContent);
    ui_pnlPasscode = lv_obj_create(ui_pnlContent);
    ui_pnlLED = lv_obj_create(ui_pnlContent);
    ui_pnlTheme = lv_obj_create(ui_pnlContent);
    ui_pnlRetroWait = lv_obj_create(ui_pnlContent);
    ui_pnlUSBFunction = lv_obj_create(ui_pnlContent);
    ui_pnlState = lv_obj_create(ui_pnlContent);
    ui_pnlVerbose = lv_obj_create(ui_pnlContent);
    ui_pnlRumble = lv_obj_create(ui_pnlContent);
    ui_pnlHDMIOutput = lv_obj_create(ui_pnlContent);
    ui_pnlUserInit = lv_obj_create(ui_pnlContent);

    ui_lblAccelerate = lv_label_create(ui_pnlAccelerate);
    ui_lblSwap = lv_label_create(ui_pnlSwap);
    ui_lblThermal = lv_label_create(ui_pnlThermal);
    ui_lblFont = lv_label_create(ui_pnlFont);
    ui_lblVolume = lv_label_create(ui_pnlVolume);
    ui_lblBrightness = lv_label_create(ui_pnlBrightness);
    ui_lblOffset = lv_label_create(ui_pnlOffset);
    ui_lblPasscode = lv_label_create(ui_pnlPasscode);
    ui_lblLED = lv_label_create(ui_pnlLED);
    ui_lblTheme = lv_label_create(ui_pnlTheme);
    ui_lblRetroWait = lv_label_create(ui_pnlRetroWait);
    ui_lblUSBFunction = lv_label_create(ui_pnlUSBFunction);
    ui_lblState = lv_label_create(ui_pnlState);
    ui_lblVerbose = lv_label_create(ui_pnlVerbose);
    ui_lblRumble = lv_label_create(ui_pnlRumble);
    ui_lblHDMIOutput = lv_label_create(ui_pnlHDMIOutput);
    ui_lblUserInit = lv_label_create(ui_pnlUserInit);

    ui_icoAccelerate = lv_img_create(ui_pnlAccelerate);
    ui_icoSwap = lv_img_create(ui_pnlSwap);
    ui_icoThermal = lv_img_create(ui_pnlThermal);
    ui_icoFont = lv_img_create(ui_pnlFont);
    ui_icoVolume = lv_img_create(ui_pnlVolume);
    ui_icoBrightness = lv_img_create(ui_pnlBrightness);
    ui_icoOffset = lv_img_create(ui_pnlOffset);
    ui_icoPasscode = lv_img_create(ui_pnlPasscode);
    ui_icoLED = lv_img_create(ui_pnlLED);
    ui_icoTheme = lv_img_create(ui_pnlTheme);
    ui_icoRetroWait = lv_img_create(ui_pnlRetroWait);
    ui_icoUSBFunction = lv_img_create(ui_pnlUSBFunction);
    ui_icoState = lv_img_create(ui_pnlState);
    ui_icoVerbose = lv_img_create(ui_pnlVerbose);
    ui_icoRumble = lv_img_create(ui_pnlRumble);
    ui_icoHDMIOutput = lv_img_create(ui_pnlHDMIOutput);
    ui_icoUserInit = lv_img_create(ui_pnlUserInit);

    ui_droAccelerate = lv_dropdown_create(ui_pnlAccelerate);
    ui_droSwap = lv_dropdown_create(ui_pnlSwap);
    ui_droThermal = lv_dropdown_create(ui_pnlThermal);
    ui_droFont = lv_dropdown_create(ui_pnlFont);
    ui_droVolume = lv_dropdown_create(ui_pnlVolume);
    ui_droBrightness = lv_dropdown_create(ui_pnlBrightness);
    ui_droOffset = lv_dropdown_create(ui_pnlOffset);
    ui_droPasscode = lv_dropdown_create(ui_pnlPasscode);
    ui_droLED = lv_dropdown_create(ui_pnlLED);
    ui_droTheme = lv_dropdown_create(ui_pnlTheme);
    ui_droRetroWait = lv_dropdown_create(ui_pnlRetroWait);
    ui_droUSBFunction = lv_dropdown_create(ui_pnlUSBFunction);
    ui_droState = lv_dropdown_create(ui_pnlState);
    ui_droVerbose = lv_dropdown_create(ui_pnlVerbose);
    ui_droRumble = lv_dropdown_create(ui_pnlRumble);
    ui_droHDMIOutput = lv_dropdown_create(ui_pnlHDMIOutput);
    ui_droUserInit = lv_dropdown_create(ui_pnlUserInit);
}
