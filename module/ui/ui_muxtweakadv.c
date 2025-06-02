#include "../../common/options.h"
#include "ui_muxtweakadv.h"

lv_obj_t *ui_pnlAccelerate_tweakadv;
lv_obj_t *ui_pnlSwap_tweakadv;
lv_obj_t *ui_pnlThermal_tweakadv;
lv_obj_t *ui_pnlVolume_tweakadv;
lv_obj_t *ui_pnlBrightness_tweakadv;
lv_obj_t *ui_pnlOffset_tweakadv;
lv_obj_t *ui_pnlPasscode_tweakadv;
lv_obj_t *ui_pnlLED_tweakadv;
lv_obj_t *ui_pnlTheme_tweakadv;
lv_obj_t *ui_pnlRetroWait_tweakadv;
lv_obj_t *ui_pnlState_tweakadv;
lv_obj_t *ui_pnlVerbose_tweakadv;
lv_obj_t *ui_pnlRumble_tweakadv;
lv_obj_t *ui_pnlUserInit_tweakadv;
lv_obj_t *ui_pnlDPADSwap_tweakadv;
lv_obj_t *ui_pnlOverdrive_tweakadv;
lv_obj_t *ui_pnlSwapfile_tweakadv;
lv_obj_t *ui_pnlZramfile_tweakadv;
lv_obj_t *ui_pnlCardMode_tweakadv;

lv_obj_t *ui_lblAccelerate_tweakadv;
lv_obj_t *ui_lblSwap_tweakadv;
lv_obj_t *ui_lblThermal_tweakadv;
lv_obj_t *ui_lblVolume_tweakadv;
lv_obj_t *ui_lblBrightness_tweakadv;
lv_obj_t *ui_lblOffset_tweakadv;
lv_obj_t *ui_lblPasscode_tweakadv;
lv_obj_t *ui_lblLED_tweakadv;
lv_obj_t *ui_lblTheme_tweakadv;
lv_obj_t *ui_lblRetroWait_tweakadv;
lv_obj_t *ui_lblState_tweakadv;
lv_obj_t *ui_lblVerbose_tweakadv;
lv_obj_t *ui_lblRumble_tweakadv;
lv_obj_t *ui_lblUserInit_tweakadv;
lv_obj_t *ui_lblDPADSwap_tweakadv;
lv_obj_t *ui_lblOverdrive_tweakadv;
lv_obj_t *ui_lblSwapfile_tweakadv;
lv_obj_t *ui_lblZramfile_tweakadv;
lv_obj_t *ui_lblCardMode_tweakadv;

lv_obj_t *ui_icoAccelerate_tweakadv;
lv_obj_t *ui_icoSwap_tweakadv;
lv_obj_t *ui_icoThermal_tweakadv;
lv_obj_t *ui_icoVolume_tweakadv;
lv_obj_t *ui_icoBrightness_tweakadv;
lv_obj_t *ui_icoOffset_tweakadv;
lv_obj_t *ui_icoPasscode_tweakadv;
lv_obj_t *ui_icoLED_tweakadv;
lv_obj_t *ui_icoTheme_tweakadv;
lv_obj_t *ui_icoRetroWait_tweakadv;
lv_obj_t *ui_icoState_tweakadv;
lv_obj_t *ui_icoVerbose_tweakadv;
lv_obj_t *ui_icoRumble_tweakadv;
lv_obj_t *ui_icoUserInit_tweakadv;
lv_obj_t *ui_icoDPADSwap_tweakadv;
lv_obj_t *ui_icoOverdrive_tweakadv;
lv_obj_t *ui_icoSwapfile_tweakadv;
lv_obj_t *ui_icoZramfile_tweakadv;
lv_obj_t *ui_icoCardMode_tweakadv;

lv_obj_t *ui_droAccelerate_tweakadv;
lv_obj_t *ui_droSwap_tweakadv;
lv_obj_t *ui_droThermal_tweakadv;
lv_obj_t *ui_droVolume_tweakadv;
lv_obj_t *ui_droBrightness_tweakadv;
lv_obj_t *ui_droOffset_tweakadv;
lv_obj_t *ui_droPasscode_tweakadv;
lv_obj_t *ui_droLED_tweakadv;
lv_obj_t *ui_droTheme_tweakadv;
lv_obj_t *ui_droRetroWait_tweakadv;
lv_obj_t *ui_droState_tweakadv;
lv_obj_t *ui_droVerbose_tweakadv;
lv_obj_t *ui_droRumble_tweakadv;
lv_obj_t *ui_droUserInit_tweakadv;
lv_obj_t *ui_droDPADSwap_tweakadv;
lv_obj_t *ui_droOverdrive_tweakadv;
lv_obj_t *ui_droSwapfile_tweakadv;
lv_obj_t *ui_droZramfile_tweakadv;
lv_obj_t *ui_droCardMode_tweakadv;

void init_muxtweakadv(lv_obj_t *ui_pnlContent) {
    CREATE_ELEMENT_ITEM(tweakadv, Accelerate);
    CREATE_ELEMENT_ITEM(tweakadv, Swap);
    CREATE_ELEMENT_ITEM(tweakadv, Thermal);
    CREATE_ELEMENT_ITEM(tweakadv, Volume);
    CREATE_ELEMENT_ITEM(tweakadv, Brightness);
    CREATE_ELEMENT_ITEM(tweakadv, Offset);
    CREATE_ELEMENT_ITEM(tweakadv, Passcode);
    CREATE_ELEMENT_ITEM(tweakadv, LED);
    CREATE_ELEMENT_ITEM(tweakadv, Theme);
    CREATE_ELEMENT_ITEM(tweakadv, RetroWait);
    CREATE_ELEMENT_ITEM(tweakadv, State);
    CREATE_ELEMENT_ITEM(tweakadv, Verbose);
    CREATE_ELEMENT_ITEM(tweakadv, Rumble);
    CREATE_ELEMENT_ITEM(tweakadv, UserInit);
    CREATE_ELEMENT_ITEM(tweakadv, DPADSwap);
    CREATE_ELEMENT_ITEM(tweakadv, Overdrive);
    CREATE_ELEMENT_ITEM(tweakadv, Swapfile);
    CREATE_ELEMENT_ITEM(tweakadv, Zramfile);
    CREATE_ELEMENT_ITEM(tweakadv, CardMode);
}

const char *volume_values[] = {
        "previous", "silent", "soft", "loud"
};

const char *brightness_values[] = {
        "previous", "low", "medium", "high"
};

const char *state_values[] = {
        "mem", "freeze"
};

const char *cardmode_values[] = {
        "deadline", "noop"
};

const int accelerate_values[] = {
        32767, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256
};

const int swap_values[] = {
        0, 64, 128, 192, 256, 320, 384, 448, 512
};

const int zram_values[] = {
        0, 64, 128, 192, 256, 320, 384, 448, 512
};
