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

#define CREATE_BAR_ITEM(MODULE, NAME) do { \
    ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent); \
    ui_pnl##NAME##Bar_##MODULE = lv_obj_create(ui_pnlContent); \
    ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE); \
    ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE); \
    ui_lbl##NAME##Value_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE); \
    ui_bar##NAME##_##MODULE = lv_bar_create(ui_pnl##NAME##Bar_##MODULE); \
    lv_label_set_text(ui_lbl##NAME##_##MODULE, ""); \
    lv_label_set_text(ui_lbl##NAME##Value_##MODULE, ""); \
    lv_obj_set_height(ui_pnl##NAME##Bar_##MODULE, 10); \
    lv_obj_set_width(ui_pnl##NAME##Bar_##MODULE, lv_pct(100)); \
    lv_obj_set_width(ui_bar##NAME##_##MODULE, lv_pct(100)); \
    lv_bar_set_range(ui_bar##NAME##_##MODULE, 0, device.MUX.WIDTH); \
    lv_obj_set_style_bg_color(ui_bar##NAME##_##MODULE, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT); \
    lv_obj_set_style_bg_opa(ui_bar##NAME##_##MODULE, 25, LV_PART_MAIN | LV_STATE_DEFAULT); \
    lv_obj_set_style_bg_color(ui_bar##NAME##_##MODULE, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    lv_obj_set_style_bg_opa(ui_bar##NAME##_##MODULE, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT); \
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

#define NETWORK_ELEMENTS \
    NETWORK(Identifier)  \
    NETWORK(Password)    \
    NETWORK(Scan)        \
    NETWORK(Type)        \
    NETWORK(Address)     \
    NETWORK(Subnet)      \
    NETWORK(Gateway)     \
    NETWORK(DNS)         \
    NETWORK(Connect)

#define OPTION_ELEMENTS \
    OPTION(Search)      \
    OPTION(Core)        \
    OPTION(Governor)    \
    OPTION(Tag)

#define POWER_ELEMENTS \
    POWER(Shutdown)    \
    POWER(Battery)     \
    POWER(IdleDisplay) \
    POWER(IdleSleep)

#define RTC_ELEMENTS \
    RTC(Year)     \
    RTC(Month)    \
    RTC(Day)      \
    RTC(Hour)     \
    RTC(Minute)   \
    RTC(Notation) \
    RTC(Timezone)

#define SEARCH_ELEMENTS  \
    SEARCH(Lookup)       \
    SEARCH(SearchLocal)  \
    SEARCH(SearchGlobal)

#define SPACE_ELEMENTS \
    SPACE(SD1)         \
    SPACE(SD2)         \
    SPACE(USB)         \
    SPACE(RFS)

#define STORAGE_ELEMENTS      \
    STORAGE(BIOS)             \
    STORAGE(Catalogue)        \
    STORAGE(Name)             \
    STORAGE(RetroArch)        \
    STORAGE(Config)           \
    STORAGE(Core)             \
    STORAGE(Collection)       \
    STORAGE(History)          \
    STORAGE(Music)            \
    STORAGE(Save)             \
    STORAGE(Screenshot)       \
    STORAGE(Theme)            \
    STORAGE(CataloguePackage) \
    STORAGE(ConfigPackage)    \
    STORAGE(BootlogoPackage)  \
    STORAGE(Language)         \
    STORAGE(Network)          \
    STORAGE(Syncthing)        \
    STORAGE(UserInit)

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
