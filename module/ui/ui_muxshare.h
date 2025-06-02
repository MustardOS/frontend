#pragma once

#define CREATE_ELEMENT_ITEM(module, name) do {                                                       \
        ui_pnl##name##_##module = lv_obj_create(ui_pnlContent);                                      \
        ui_lbl##name##_##module = lv_label_create(ui_pnl##name##_##module);                          \
        lv_label_set_text(ui_lbl##name##_##module, "");                                              \
        ui_ico##name##_##module = lv_img_create(ui_pnl##name##_##module);                            \
        ui_dro##name##_##module = lv_dropdown_create(ui_pnl##name##_##module);                       \
        lv_dropdown_clear_options(ui_dro##name##_##module);                                          \
        lv_obj_set_style_text_opa(ui_dro##name##_##module, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    } while (0)

#define KIOSK_ELEMENTS \
    KIOSK(Enable)      \
    KIOSK(Archive)     \
    KIOSK(Task)        \
    KIOSK(Custom)      \
    KIOSK(Language)    \
    KIOSK(Network)     \
    KIOSK(Storage)     \
    KIOSK(WebServ)     \
    KIOSK(Core)        \
    KIOSK(Governor)    \
    KIOSK(Option)      \
    KIOSK(RetroArch)   \
    KIOSK(Search)      \
    KIOSK(Tag)         \
    KIOSK(Bootlogo)    \
    KIOSK(Catalogue)   \
    KIOSK(RAConfig)    \
    KIOSK(Theme)       \
    KIOSK(Clock)       \
    KIOSK(Timezone)    \
    KIOSK(Apps)        \
    KIOSK(Config)      \
    KIOSK(Explore)     \
    KIOSK(Collection)  \
    KIOSK(History)     \
    KIOSK(Info)        \
    KIOSK(Advanced)    \
    KIOSK(General)     \
    KIOSK(HDMI)        \
    KIOSK(Power)       \
    KIOSK(Visual)

#define TWEAKADV_ELEMENTS \
    TWEAKADV(Accelerate)  \
    TWEAKADV(Swap)        \
    TWEAKADV(Thermal)     \
    TWEAKADV(Volume)      \
    TWEAKADV(Brightness)  \
    TWEAKADV(Offset)      \
    TWEAKADV(Passcode)    \
    TWEAKADV(LED)         \
    TWEAKADV(Theme)       \
    TWEAKADV(RetroWait)   \
    TWEAKADV(State)       \
    TWEAKADV(Verbose)     \
    TWEAKADV(Rumble)      \
    TWEAKADV(UserInit)    \
    TWEAKADV(DPADSwap)    \
    TWEAKADV(Overdrive)   \
    TWEAKADV(Swapfile)    \
    TWEAKADV(Zramfile)    \
    TWEAKADV(CardMode)    \

#define VISUAL_ELEMENTS           \
    VISUAL(Battery)               \
    VISUAL(Clock)                 \
    VISUAL(Network)               \
    VISUAL(Name)                  \
    VISUAL(Dash)                  \
    VISUAL(FriendlyFolder)        \
    VISUAL(TheTitleFormat)        \
    VISUAL(TitleIncludeRootDrive) \
    VISUAL(FolderItemCount)       \
    VISUAL(DisplayEmptyFolder)    \
    VISUAL(MenuCounterFolder)     \
    VISUAL(MenuCounterFile)       \
    VISUAL(Hidden)                \
    VISUAL(OverlayImage)          \
    VISUAL(OverlayTransparency)

#define WEBSERV_ELEMENTS \
    WEBSERV(SSHD)        \
    WEBSERV(SFTPGo)      \
    WEBSERV(TTYD)        \
    WEBSERV(Syncthing)   \
    WEBSERV(RSLSync)     \
    WEBSERV(NTP)         \
    WEBSERV(Tailscaled)
