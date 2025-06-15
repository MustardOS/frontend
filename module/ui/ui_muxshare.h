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

#define CREATE_STATIC_ITEM(MODULE, NAME) do {                                                        \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                                      \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                          \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                                              \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);                            \
    } while (0)

#define CREATE_VALUE_ITEM(MODULE, NAME) do {                                     \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                  \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);      \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                          \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);        \
        ui_lbl##NAME##Value_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE); \
        lv_label_set_text(ui_lbl##NAME##Value_##MODULE, "");                     \
    } while (0)

#define CREATE_BAR_ITEM(MODULE, NAME) do {                                                                                           \
    ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                                                                          \
    ui_pnl##NAME##Bar_##MODULE = lv_obj_create(ui_pnlContent);                                                                       \
    ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                                                              \
    ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);                                                                \
    ui_lbl##NAME##Value_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                                                         \
    ui_bar##NAME##_##MODULE = lv_bar_create(ui_pnl##NAME##Bar_##MODULE);                                                             \
    lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                                                                                  \
    lv_label_set_text(ui_lbl##NAME##Value_##MODULE, "");                                                                             \
    lv_obj_set_height(ui_pnl##NAME##Bar_##MODULE, 10);                                                                               \
    lv_obj_set_width(ui_pnl##NAME##Bar_##MODULE, lv_pct(100));                                                                       \
    lv_obj_set_width(ui_bar##NAME##_##MODULE, lv_pct(100));                                                                          \
    lv_bar_set_range(ui_bar##NAME##_##MODULE, 0, device.MUX.WIDTH);                                                                  \
    lv_obj_set_style_bg_color(ui_bar##NAME##_##MODULE, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);      \
    lv_obj_set_style_bg_opa(ui_bar##NAME##_##MODULE, 25, LV_PART_MAIN | LV_STATE_DEFAULT);                                           \
    lv_obj_set_style_bg_color(ui_bar##NAME##_##MODULE, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    lv_obj_set_style_bg_opa(ui_bar##NAME##_##MODULE, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);                                     \
} while (0)

#define CONNECT_ELEMENTS                \
    CONNECT(Network,     "network")     \
    CONNECT(Services,    "service")     \
    CONNECT(Bluetooth,   "bluetooth")   \
    CONNECT(UsbFunction, "usbfunction")

#define CONFIG_ELEMENTS            \
    CONFIG(General,   "general")   \
    CONFIG(Connect,   "connect")   \
    CONFIG(Custom,    "custom")    \
    CONFIG(Interface, "interface") \
    CONFIG(Language,  "language")  \
    CONFIG(Power,     "power")     \
    CONFIG(Storage,   "storage")   \
    CONFIG(Backup,    "backup")

#define CUSTOM_ELEMENTS                   \
    CUSTOM(Bootlogo,        "bootlogo")   \
    CUSTOM(Catalogue,       "catalogue")  \
    CUSTOM(Config,          "config")     \
    CUSTOM(Theme,           "theme")      \
    CUSTOM(ThemeResolution, "resolution") \
    CUSTOM(ThemeAlternate,  "alternate")  \
    CUSTOM(Animation,       "animation")  \
    CUSTOM(Music,           "music")      \
    CUSTOM(BlackFade,       "blackfade")  \
    CUSTOM(BoxArtImage,     "boxart")     \
    CUSTOM(BoxArtAlign,     "align")      \
    CUSTOM(LaunchSplash,    "splash")     \
    CUSTOM(Font,            "font")       \
    CUSTOM(Sound,           "sound")      \
    CUSTOM(Chime,           "chime")

#define DANGER_ELEMENTS                  \
    DANGER(VmSwap,        "vmswap")      \
    DANGER(DirtyRatio,    "dirty-ratio") \
    DANGER(DirtyBack,     "dirty-back")  \
    DANGER(CachePressure, "cache")       \
    DANGER(NoMerge,       "merge")       \
    DANGER(NrRequests,    "requests")    \
    DANGER(ReadAhead,     "readahead")   \
    DANGER(PageCluster,   "cluster")     \
    DANGER(TimeSlice,     "timeslice")   \
    DANGER(IoStats,       "iostats")     \
    DANGER(IdleFlush,     "idleflush")   \
    DANGER(ChildFirst,    "child")       \
    DANGER(TuneScale,     "tunescale")

#define INFO_ELEMENTS              \
    INFO(Screenshot, "screenshot") \
    INFO(Space,      "space")      \
    INFO(Tester,     "tester")     \
    INFO(SysInfo,    "sysinfo")    \
    INFO(NetInfo,    "netinfo")    \
    INFO(Credit,     "credit")

#define HDMI_ELEMENTS              \
    HDMI(Resolution, "resolution") \
    HDMI(Space,      "space")      \
    HDMI(Depth,      "depth")      \
    HDMI(Range,      "range")      \
    HDMI(Scan,       "scan")       \
    HDMI(Audio,      "audio")

#define KIOSK_ELEMENTS              \
    KIOSK(Enable,     "enable")     \
    KIOSK(Archive,    "archive")    \
    KIOSK(Task,       "task")       \
    KIOSK(Custom,     "custom")     \
    KIOSK(Language,   "language")   \
    KIOSK(Network,    "network")    \
    KIOSK(Storage,    "storage")    \
    KIOSK(WebServ,    "webserv")    \
    KIOSK(Core,       "core")       \
    KIOSK(Governor,   "governor")   \
    KIOSK(Option,     "option")     \
    KIOSK(RetroArch,  "retroarch")  \
    KIOSK(Search,     "search")     \
    KIOSK(Tag,        "tag")        \
    KIOSK(Bootlogo,   "bootlogo")   \
    KIOSK(Catalogue,  "catalogue")  \
    KIOSK(RAConfig,   "raconfig")   \
    KIOSK(Theme,      "theme")      \
    KIOSK(Clock,      "clock")      \
    KIOSK(Timezone,   "timezone")   \
    KIOSK(Apps,       "apps")       \
    KIOSK(Config,     "config")     \
    KIOSK(Explore,    "explore")    \
    KIOSK(Collection, "collection") \
    KIOSK(History,    "history")    \
    KIOSK(Info,       "info")       \
    KIOSK(Advanced,   "advanced")   \
    KIOSK(General,    "general")    \
    KIOSK(Hdmi,       "hdmi")       \
    KIOSK(Power,      "power")      \
    KIOSK(Visual,     "visual")

#define LAUNCH_ELEMENTS              \
    LAUNCH(Explore,    "explore")    \
    LAUNCH(Collection, "collection") \
    LAUNCH(History,    "history")    \
    LAUNCH(Apps,       "apps")       \
    LAUNCH(Info,       "info")       \
    LAUNCH(Config,     "config")     \
    LAUNCH(Reboot,     "reboot")     \
    LAUNCH(Shutdown,   "shutdown")

#define NETINFO_ELEMENTS            \
    NETINFO(Hostname,  "hostname")  \
    NETINFO(Mac,       "mac")       \
    NETINFO(Ip,        "ip")        \
    NETINFO(Ssid,      "ssid")      \
    NETINFO(Gateway,   "gateway")   \
    NETINFO(Dns,       "dns")       \
    NETINFO(Signal,    "signal")    \
    NETINFO(Channel,   "channel")   \
    NETINFO(AcTraffic, "actraffic") \
    NETINFO(TpTraffic, "tptraffic")

#define NETWORK_ELEMENTS              \
    NETWORK(Identifier, "identifier") \
    NETWORK(Password,   "password")   \
    NETWORK(Scan,       "scan")       \
    NETWORK(Type,       "type")       \
    NETWORK(Address,    "address")    \
    NETWORK(Subnet,     "subnet")     \
    NETWORK(Gateway,    "gateway")    \
    NETWORK(Dns,        "dns")        \
    NETWORK(Connect,    "connect")

#define OPTION_ELEMENTS          \
    OPTION(Search,   "search")   \
    OPTION(Core,     "core")     \
    OPTION(Governor, "governor") \
    OPTION(Tag,      "tag")

#define POWER_ELEMENTS \
    POWER(Shutdown,    "shutdown")     \
    POWER(Battery,     "battery")      \
    POWER(IdleDisplay, "idle_display") \
    POWER(IdleSleep,   "idle_sleep")

#define RTC_ELEMENTS          \
    RTC(Year,     "year")     \
    RTC(Month,    "month")    \
    RTC(Day,      "day")      \
    RTC(Hour,     "hour")     \
    RTC(Minute,   "minute")   \
    RTC(Notation, "notation") \
    RTC(Timezone, "timezone")

#define SEARCH_ELEMENTS            \
    SEARCH(Lookup,       "lookup") \
    SEARCH(SearchLocal,  "local")  \
    SEARCH(SearchGlobal, "global")

#define SPACE_ELEMENTS            \
    SPACE(Primary,   "primary")   \
    SPACE(Secondary, "secondary") \
    SPACE(External,  "external")  \
    SPACE(System,    "system")

#define STORAGE_ELEMENTS                        \
    STORAGE(Bios,             "bios")           \
    STORAGE(Catalogue,        "catalogue")      \
    STORAGE(Name,             "name")           \
    STORAGE(RetroArch,        "retroarch")      \
    STORAGE(Config,           "config")         \
    STORAGE(Core,             "core")           \
    STORAGE(Collection,       "collection")     \
    STORAGE(History,          "history")        \
    STORAGE(Music,            "music")          \
    STORAGE(Save,             "save")           \
    STORAGE(Screenshot,       "screenshot")     \
    STORAGE(Theme,            "theme")          \
    STORAGE(CataloguePackage, "pack-catalogue") \
    STORAGE(ConfigPackage,    "pack-config")    \
    STORAGE(BootlogoPackage,  "pack-bootlogo")  \
    STORAGE(Language,         "language")       \
    STORAGE(Network,          "network")        \
    STORAGE(Syncthing,        "syncthing")      \
    STORAGE(UserInit,         "userinit")

#define BACKUP_ELEMENTS                        \
    BACKUP(Bios, "bios")                       \
    BACKUP(Catalogue, "catalogue")             \
    BACKUP(Name, "name")                       \
    BACKUP(RetroArch, "retroarch")             \
    BACKUP(Config, "config")                   \
    BACKUP(Core, "core")                       \
    BACKUP(Collection, "collection")           \
    BACKUP(History, "history")                 \
    BACKUP(Music, "music")                     \
    BACKUP(Save, "save")                       \
    BACKUP(Screenshot, "screenshot")           \
    BACKUP(Theme, "theme")                     \
    BACKUP(CataloguePackage, "pack-catalogue") \
    BACKUP(ConfigPackage, "pack-config")       \
    BACKUP(BootlogoPackage, "pack-bootlogo")   \
    BACKUP(Language, "language")               \
    BACKUP(Network, "network")                 \
    BACKUP(Syncthing, "syncthing")             \
    BACKUP(UserInit, "userinit")               \
    BACKUP(BackupTarget, "backuptarget")       \
    BACKUP(StartBackup, "startbackup")

#define SYSINFO_ELEMENTS          \
    SYSINFO(Version,  "version")  \
    SYSINFO(Device,   "device")   \
    SYSINFO(Kernel,   "kernel")   \
    SYSINFO(Uptime,   "uptime")   \
    SYSINFO(Cpu,      "cpu")      \
    SYSINFO(Speed,    "speed")    \
    SYSINFO(Governor, "governor") \
    SYSINFO(Memory,   "memory")   \
    SYSINFO(Temp,     "temp")     \
    SYSINFO(Capacity, "capacity") \
    SYSINFO(Voltage,  "voltage")

#define TWEAKADV_ELEMENTS              \
    TWEAKADV(Accelerate, "accelerate") \
    TWEAKADV(Swap,       "swap")       \
    TWEAKADV(Thermal,    "thermal")    \
    TWEAKADV(Volume,     "volume")     \
    TWEAKADV(Brightness, "brightness") \
    TWEAKADV(Offset,     "offset")     \
    TWEAKADV(Passcode,   "lock")       \
    TWEAKADV(Led,        "led")        \
    TWEAKADV(Theme,      "theme")      \
    TWEAKADV(RetroWait,  "retrowait")  \
    TWEAKADV(State,      "state")      \
    TWEAKADV(Verbose,    "verbose")    \
    TWEAKADV(Rumble,     "rumble")     \
    TWEAKADV(UserInit,   "userinit")   \
    TWEAKADV(DpadSwap,   "dpadswap")   \
    TWEAKADV(Overdrive,  "overdrive")  \
    TWEAKADV(Swapfile,   "swapfile")   \
    TWEAKADV(Zramfile,   "zramfile")   \
    TWEAKADV(CardMode,   "cardmode")

#define TWEAKGEN_ELEMENTS              \
    TWEAKGEN(Rtc,        "clock")      \
    TWEAKGEN(Hdmi,       "hdmi")       \
    TWEAKGEN(Advanced,   "advanced")   \
    TWEAKGEN(Brightness, "brightness") \
    TWEAKGEN(Volume,     "volume")     \
    TWEAKGEN(Colour,     "colour")     \
    TWEAKGEN(Startup,    "startup")

#define VISUAL_ELEMENTS                                    \
    VISUAL(Battery,               "battery")               \
    VISUAL(Clock,                 "clock")                 \
    VISUAL(Network,               "network")               \
    VISUAL(Name,                  "name")                  \
    VISUAL(Dash,                  "dash")                  \
    VISUAL(FriendlyFolder,        "friendlyfolder")        \
    VISUAL(TheTitleFormat,        "thetitleformat")        \
    VISUAL(TitleIncludeRootDrive, "titleincluderootdrive") \
    VISUAL(FolderItemCount,       "folderitemcount")       \
    VISUAL(DisplayEmptyFolder,    "folderempty")           \
    VISUAL(MenuCounterFolder,     "counterfolder")         \
    VISUAL(MenuCounterFile,       "counterfile")           \
    VISUAL(Hidden,                "hidden")                \
    VISUAL(OverlayImage,          "overlayimage")          \
    VISUAL(OverlayTransparency,   "overlaytransparency")

#define WEBSERV_ELEMENTS              \
    WEBSERV(Sshd,       "sshd")       \
    WEBSERV(SftpGo,     "sftpgo")     \
    WEBSERV(Ttyd,       "ttyd")       \
    WEBSERV(Syncthing,  "syncthing")  \
    WEBSERV(RslSync,    "rslsync")    \
    WEBSERV(Ntp,        "ntp")        \
    WEBSERV(Tailscaled, "tailscaled")
