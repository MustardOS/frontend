#include "ui_muxkiosk.h"

lv_obj_t *ui_pnlEnable_kiosk;
lv_obj_t *ui_pnlArchive_kiosk;
lv_obj_t *ui_pnlTask_kiosk;
lv_obj_t *ui_pnlCustom_kiosk;
lv_obj_t *ui_pnlLanguage_kiosk;
lv_obj_t *ui_pnlNetwork_kiosk;
lv_obj_t *ui_pnlStorage_kiosk;
lv_obj_t *ui_pnlWebServ_kiosk;
lv_obj_t *ui_pnlCore_kiosk;
lv_obj_t *ui_pnlGovernor_kiosk;
lv_obj_t *ui_pnlOption_kiosk;
lv_obj_t *ui_pnlRetroArch_kiosk;
lv_obj_t *ui_pnlSearch_kiosk;
lv_obj_t *ui_pnlTag_kiosk;
lv_obj_t *ui_pnlBootlogo_kiosk;
lv_obj_t *ui_pnlCatalogue_kiosk;
lv_obj_t *ui_pnlRAConfig_kiosk;
lv_obj_t *ui_pnlTheme_kiosk;
lv_obj_t *ui_pnlClock_kiosk;
lv_obj_t *ui_pnlTimezone_kiosk;
lv_obj_t *ui_pnlApps_kiosk;
lv_obj_t *ui_pnlConfig_kiosk;
lv_obj_t *ui_pnlExplore_kiosk;
lv_obj_t *ui_pnlCollection_kiosk;
lv_obj_t *ui_pnlHistory_kiosk;
lv_obj_t *ui_pnlInfo_kiosk;
lv_obj_t *ui_pnlAdvanced_kiosk;
lv_obj_t *ui_pnlGeneral_kiosk;
lv_obj_t *ui_pnlHDMI_kiosk;
lv_obj_t *ui_pnlPower_kiosk;
lv_obj_t *ui_pnlVisual_kiosk;

lv_obj_t *ui_lblEnable_kiosk;
lv_obj_t *ui_lblArchive_kiosk;
lv_obj_t *ui_lblTask_kiosk;
lv_obj_t *ui_lblCustom_kiosk;
lv_obj_t *ui_lblLanguage_kiosk;
lv_obj_t *ui_lblNetwork_kiosk;
lv_obj_t *ui_lblStorage_kiosk;
lv_obj_t *ui_lblWebServ_kiosk;
lv_obj_t *ui_lblCore_kiosk;
lv_obj_t *ui_lblGovernor_kiosk;
lv_obj_t *ui_lblOption_kiosk;
lv_obj_t *ui_lblRetroArch_kiosk;
lv_obj_t *ui_lblSearch_kiosk;
lv_obj_t *ui_lblTag_kiosk;
lv_obj_t *ui_lblBootlogo_kiosk;
lv_obj_t *ui_lblCatalogue_kiosk;
lv_obj_t *ui_lblRAConfig_kiosk;
lv_obj_t *ui_lblTheme_kiosk;
lv_obj_t *ui_lblClock_kiosk;
lv_obj_t *ui_lblTimezone_kiosk;
lv_obj_t *ui_lblApps_kiosk;
lv_obj_t *ui_lblConfig_kiosk;
lv_obj_t *ui_lblExplore_kiosk;
lv_obj_t *ui_lblCollection_kiosk;
lv_obj_t *ui_lblHistory_kiosk;
lv_obj_t *ui_lblInfo_kiosk;
lv_obj_t *ui_lblAdvanced_kiosk;
lv_obj_t *ui_lblGeneral_kiosk;
lv_obj_t *ui_lblHDMI_kiosk;
lv_obj_t *ui_lblPower_kiosk;
lv_obj_t *ui_lblVisual_kiosk;

lv_obj_t *ui_icoEnable_kiosk;
lv_obj_t *ui_icoArchive_kiosk;
lv_obj_t *ui_icoTask_kiosk;
lv_obj_t *ui_icoCustom_kiosk;
lv_obj_t *ui_icoLanguage_kiosk;
lv_obj_t *ui_icoNetwork_kiosk;
lv_obj_t *ui_icoStorage_kiosk;
lv_obj_t *ui_icoWebServ_kiosk;
lv_obj_t *ui_icoCore_kiosk;
lv_obj_t *ui_icoGovernor_kiosk;
lv_obj_t *ui_icoOption_kiosk;
lv_obj_t *ui_icoRetroArch_kiosk;
lv_obj_t *ui_icoSearch_kiosk;
lv_obj_t *ui_icoTag_kiosk;
lv_obj_t *ui_icoBootlogo_kiosk;
lv_obj_t *ui_icoCatalogue_kiosk;
lv_obj_t *ui_icoRAConfig_kiosk;
lv_obj_t *ui_icoTheme_kiosk;
lv_obj_t *ui_icoClock_kiosk;
lv_obj_t *ui_icoTimezone_kiosk;
lv_obj_t *ui_icoApps_kiosk;
lv_obj_t *ui_icoConfig_kiosk;
lv_obj_t *ui_icoExplore_kiosk;
lv_obj_t *ui_icoCollection_kiosk;
lv_obj_t *ui_icoHistory_kiosk;
lv_obj_t *ui_icoInfo_kiosk;
lv_obj_t *ui_icoAdvanced_kiosk;
lv_obj_t *ui_icoGeneral_kiosk;
lv_obj_t *ui_icoHDMI_kiosk;
lv_obj_t *ui_icoPower_kiosk;
lv_obj_t *ui_icoVisual_kiosk;

lv_obj_t *ui_droEnable_kiosk;
lv_obj_t *ui_droArchive_kiosk;
lv_obj_t *ui_droTask_kiosk;
lv_obj_t *ui_droCustom_kiosk;
lv_obj_t *ui_droLanguage_kiosk;
lv_obj_t *ui_droNetwork_kiosk;
lv_obj_t *ui_droStorage_kiosk;
lv_obj_t *ui_droWebServ_kiosk;
lv_obj_t *ui_droCore_kiosk;
lv_obj_t *ui_droGovernor_kiosk;
lv_obj_t *ui_droOption_kiosk;
lv_obj_t *ui_droRetroArch_kiosk;
lv_obj_t *ui_droSearch_kiosk;
lv_obj_t *ui_droTag_kiosk;
lv_obj_t *ui_droBootlogo_kiosk;
lv_obj_t *ui_droCatalogue_kiosk;
lv_obj_t *ui_droRAConfig_kiosk;
lv_obj_t *ui_droTheme_kiosk;
lv_obj_t *ui_droClock_kiosk;
lv_obj_t *ui_droTimezone_kiosk;
lv_obj_t *ui_droApps_kiosk;
lv_obj_t *ui_droConfig_kiosk;
lv_obj_t *ui_droExplore_kiosk;
lv_obj_t *ui_droCollection_kiosk;
lv_obj_t *ui_droHistory_kiosk;
lv_obj_t *ui_droInfo_kiosk;
lv_obj_t *ui_droAdvanced_kiosk;
lv_obj_t *ui_droGeneral_kiosk;
lv_obj_t *ui_droHDMI_kiosk;
lv_obj_t *ui_droPower_kiosk;
lv_obj_t *ui_droVisual_kiosk;

void init_muxkiosk(lv_obj_t *ui_pnlContent) {
#define CREATE_ELEMENT_ITEM(module, name) do {                                                       \
        ui_pnl##name##_##module = lv_obj_create(ui_pnlContent);                                      \
        ui_lbl##name##_##module = lv_label_create(ui_pnl##name##_##module);                          \
        lv_label_set_text(ui_lbl##name##_##module, "");                                              \
        ui_ico##name##_##module = lv_img_create(ui_pnl##name##_##module);                            \
        ui_dro##name##_##module = lv_dropdown_create(ui_pnl##name##_##module);                       \
        lv_dropdown_clear_options(ui_dro##name##_##module);                                          \
        lv_obj_set_style_text_opa(ui_dro##name##_##module, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    } while (0)

    CREATE_ELEMENT_ITEM(kiosk, Enable);
    CREATE_ELEMENT_ITEM(kiosk, Archive);
    CREATE_ELEMENT_ITEM(kiosk, Task);
    CREATE_ELEMENT_ITEM(kiosk, Custom);
    CREATE_ELEMENT_ITEM(kiosk, Language);
    CREATE_ELEMENT_ITEM(kiosk, Network);
    CREATE_ELEMENT_ITEM(kiosk, Storage);
    CREATE_ELEMENT_ITEM(kiosk, WebServ);
    CREATE_ELEMENT_ITEM(kiosk, Core);
    CREATE_ELEMENT_ITEM(kiosk, Governor);
    CREATE_ELEMENT_ITEM(kiosk, Option);
    CREATE_ELEMENT_ITEM(kiosk, RetroArch);
    CREATE_ELEMENT_ITEM(kiosk, Search);
    CREATE_ELEMENT_ITEM(kiosk, Tag);
    CREATE_ELEMENT_ITEM(kiosk, Bootlogo);
    CREATE_ELEMENT_ITEM(kiosk, Catalogue);
    CREATE_ELEMENT_ITEM(kiosk, RAConfig);
    CREATE_ELEMENT_ITEM(kiosk, Theme);
    CREATE_ELEMENT_ITEM(kiosk, Clock);
    CREATE_ELEMENT_ITEM(kiosk, Timezone);
    CREATE_ELEMENT_ITEM(kiosk, Apps);
    CREATE_ELEMENT_ITEM(kiosk, Config);
    CREATE_ELEMENT_ITEM(kiosk, Explore);
    CREATE_ELEMENT_ITEM(kiosk, Collection);
    CREATE_ELEMENT_ITEM(kiosk, History);
    CREATE_ELEMENT_ITEM(kiosk, Info);
    CREATE_ELEMENT_ITEM(kiosk, Advanced);
    CREATE_ELEMENT_ITEM(kiosk, General);
    CREATE_ELEMENT_ITEM(kiosk, HDMI);
    CREATE_ELEMENT_ITEM(kiosk, Power);
    CREATE_ELEMENT_ITEM(kiosk, Visual);

#undef CREATE_ELEMENT_ITEM
}
