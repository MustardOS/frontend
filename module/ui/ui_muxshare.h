#pragma once

#define CREATE_OPTION_ITEM(MODULE, NAME) do {                                                        \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                                      \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                          \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                                              \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);                            \
        ui_dro##NAME##_##MODULE = lv_dropdown_create(ui_pnl##NAME##_##MODULE);                       \
        lv_dropdown_clear_options(ui_dro##NAME##_##MODULE);                                          \
        lv_obj_set_style_text_opa(ui_dro##NAME##_##MODULE, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    } while (0)

#define CREATE_VALUE_ITEM(MODULE, NAME) do {                                     \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                  \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);      \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                          \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);        \
        ui_lbl##NAME##Value_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE); \
        lv_label_set_text(ui_lbl##NAME##Value_##MODULE, "");                     \
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

#define SYSINFO_ELEMENTS \
    SYSINFO(Version)     \
    SYSINFO(Device)      \
    SYSINFO(Kernel)      \
    SYSINFO(Uptime)      \
    SYSINFO(CPU)         \
    SYSINFO(Speed)       \
    SYSINFO(Governor)    \
    SYSINFO(Memory)      \
    SYSINFO(Temp)        \
    SYSINFO(Capacity)    \
    SYSINFO(Voltage)

#define TWEAKGEN_ELEMENTS \
    TWEAKGEN(RTC)         \
    TWEAKGEN(HDMI)        \
    TWEAKGEN(Advanced)    \
    TWEAKGEN(Brightness)  \
    TWEAKGEN(Volume)      \
    TWEAKGEN(Colour)      \
    TWEAKGEN(Startup)

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
    TWEAKADV(CardMode)

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
