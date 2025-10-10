#pragma once

#define CREATE_OPTION_ITEM(MODULE, NAME) do {                                                   \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                                 \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                     \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                                         \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);                       \
        ui_dro##NAME##_##MODULE = lv_dropdown_create(ui_pnl##NAME##_##MODULE);                  \
        lv_dropdown_clear_options(ui_dro##NAME##_##MODULE);                                     \
        lv_obj_set_style_text_opa(ui_dro##NAME##_##MODULE, LV_OPA_TRANSP, MU_OBJ_INDI_DEFAULT); \
    } while (0)

#define CREATE_STATIC_ITEM(MODULE, NAME) do {                               \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);             \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE); \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                     \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);   \
    } while (0)

#define CREATE_VALUE_ITEM(MODULE, NAME) do {                                     \
        ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                  \
        ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);      \
        lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                          \
        ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);        \
        ui_lbl##NAME##Value_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE); \
        lv_label_set_text(ui_lbl##NAME##Value_##MODULE, "");                     \
    } while (0)

#define CREATE_BAR_ITEM(MODULE, NAME) do {                                                                          \
    ui_pnl##NAME##_##MODULE = lv_obj_create(ui_pnlContent);                                                         \
    ui_pnl##NAME##Bar_##MODULE = lv_obj_create(ui_pnlContent);                                                      \
    ui_lbl##NAME##_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                                             \
    ui_ico##NAME##_##MODULE = lv_img_create(ui_pnl##NAME##_##MODULE);                                               \
    ui_lbl##NAME##Value_##MODULE = lv_label_create(ui_pnl##NAME##_##MODULE);                                        \
    ui_bar##NAME##_##MODULE = lv_bar_create(ui_pnl##NAME##Bar_##MODULE);                                            \
    lv_label_set_text(ui_lbl##NAME##_##MODULE, "");                                                                 \
    lv_label_set_text(ui_lbl##NAME##Value_##MODULE, "");                                                            \
    lv_obj_set_height(ui_pnl##NAME##Bar_##MODULE, 10);                                                              \
    lv_obj_set_width(ui_pnl##NAME##Bar_##MODULE, lv_pct(100));                                                      \
    lv_obj_set_width(ui_bar##NAME##_##MODULE, lv_pct(100));                                                         \
    lv_bar_set_range(ui_bar##NAME##_##MODULE, 0, device.MUX.WIDTH);                                                 \
    lv_obj_set_style_bg_color(ui_bar##NAME##_##MODULE, lv_color_hex(theme.VERBOSE_BOOT.TEXT), MU_OBJ_MAIN_DEFAULT); \
    lv_obj_set_style_bg_opa(ui_bar##NAME##_##MODULE, 25, MU_OBJ_MAIN_DEFAULT);                                      \
    lv_obj_set_style_bg_color(ui_bar##NAME##_##MODULE, lv_color_hex(theme.VERBOSE_BOOT.TEXT), MU_OBJ_INDI_DEFAULT); \
    lv_obj_set_style_bg_opa(ui_bar##NAME##_##MODULE, LV_OPA_COVER, MU_OBJ_INDI_DEFAULT);                            \
} while (0)

#define APPCON_ELEMENTS          \
    APPCON(Governor, "governor") \
    APPCON(Control,  "control")

#define BACKUP_ELEMENTS              \
    BACKUP(Apps,       "apps")       \
    BACKUP(Bios,       "bios")       \
    BACKUP(Catalogue,  "catalogue")  \
    BACKUP(Cheats,     "cheats")     \
    BACKUP(Collection, "collection") \
    BACKUP(Config,     "config")     \
    BACKUP(History,    "history")    \
    BACKUP(Init,       "init")       \
    BACKUP(Music,      "music")      \
    BACKUP(Name,       "name")       \
    BACKUP(Network,    "network")    \
    BACKUP(Overlays,   "overlays")   \
    BACKUP(Override,   "override")   \
    BACKUP(Package,    "package")    \
    BACKUP(Save,       "save")       \
    BACKUP(Screenshot, "screenshot") \
    BACKUP(Shaders,    "shaders")    \
    BACKUP(Syncthing,  "syncthing")  \
    BACKUP(Theme,      "theme")      \
    BACKUP(Track,      "track")      \
    BACKUP(Target,     "target")     \
    BACKUP(Merge,      "merge")      \
    BACKUP(Start,      "start")

#define CONNECT_ELEMENTS                \
    CONNECT(Network,     "network")     \
    CONNECT(NetAdv,      "netadv")      \
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

#define CUSTOM_ELEMENTS                         \
    CUSTOM(Catalogue,       "catalogue")        \
    CUSTOM(Config,          "config")           \
    CUSTOM(Theme,           "theme")            \
    CUSTOM(ThemeResolution, "resolution")       \
    CUSTOM(ThemeAlternate,  "alternate")        \
    CUSTOM(Animation,       "animation")        \
    CUSTOM(Music,           "music")            \
    CUSTOM(BlackFade,       "blackfade")        \
    CUSTOM(LaunchSwap,      "launch_swap")      \
    CUSTOM(Shuffle,         "shuffle")          \
    CUSTOM(BoxArtImage,     "boxart")           \
    CUSTOM(BoxArtAlign,     "align")            \
    CUSTOM(LaunchSplash,    "splash")           \
    CUSTOM(BoxArtHide,      "boxarthide")       \
    CUSTOM(GridModeContent, "gridmodecontent")  \
    CUSTOM(Font,            "font")             \
    CUSTOM(Sound,           "sound")            \
    CUSTOM(Chime,           "chime")

#define THEMEFILTER_ELEMENTS               \
    THEMEFILTER(AllThemes,   "theme")      \
    THEMEFILTER(Grid,        "grid")       \
    THEMEFILTER(Hdmi,        "hdmi")       \
    THEMEFILTER(Language,    "language")

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
    DANGER(TuneScale,     "tunescale")   \
    DANGER(CardMode,      "cardmode")    \
    DANGER(State,         "state")

#define DEVICE_ELEMENTS                 \
    DEVICE(HasBluetooth,  "bluetooth")  \
    DEVICE(HasRgb,        "rgb")        \
    DEVICE(HasDebugFs,    "debugfs")    \
    DEVICE(HasHdmi,       "hdmi")       \
    DEVICE(HasLid,        "lid")        \
    DEVICE(HasNetwork,    "network")    \
    DEVICE(HasPortmaster, "portmaster")

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

#define INSTALL_ELEMENTS          \
    INSTALL(Rtc,      "clock")    \
    INSTALL(Language, "language") \
    INSTALL(Shutdown, "shutdown") \
    INSTALL(Install,  "install")

#define KIOSK_ELEMENTS              \
    KIOSK(Enable,     "enable")     \
    KIOSK(Message,    "message")    \
    KIOSK(Archive,    "archive")    \
    KIOSK(Task,       "task")       \
    KIOSK(Custom,     "custom")     \
    KIOSK(Language,   "language")   \
    KIOSK(Network,    "network")    \
    KIOSK(Storage,    "storage")    \
    KIOSK(Backup,     "backup")     \
    KIOSK(NetAdv,     "netadv")     \
    KIOSK(WebServ,    "webserv")    \
    KIOSK(Core,       "core")       \
    KIOSK(Governor,   "governor")   \
    KIOSK(Control,    "control")    \
    KIOSK(Option,     "option")     \
    KIOSK(RetroArch,  "retroarch")  \
    KIOSK(Search,     "search")     \
    KIOSK(Tag,        "tag")        \
    KIOSK(Catalogue,  "catalogue")  \
    KIOSK(RAConfig,   "raconfig")   \
    KIOSK(Theme,      "theme")      \
    KIOSK(ThemeDown,  "theme_down") \
    KIOSK(Clock,      "clock")      \
    KIOSK(Timezone,   "timezone")   \
    KIOSK(Apps,       "apps")       \
    KIOSK(Config,     "config")     \
    KIOSK(Explore,    "explore")    \
    KIOSK(CollectMod, "collectmod") \
    KIOSK(CollectAdd, "collectadd") \
    KIOSK(CollectNew, "collectnew") \
    KIOSK(CollectRem, "collectrem") \
    KIOSK(CollectAcc, "collectacc") \
    KIOSK(HistoryMod, "historymod") \
    KIOSK(HistoryRem, "historyrem") \
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

#define NETADV_ELEMENTS            \
    NETADV(Monitor,   "monitor")   \
    NETADV(Boot,      "boot")      \
    NETADV(Compat,    "compat")    \
    NETADV(AsyncLoad, "asyncload") \
    NETADV(Wait,      "wait")      \
    NETADV(Retry,     "retry")

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
    OPTION(Control,  "control")  \
    OPTION(Tag,      "tag")

#define POWER_ELEMENTS \
    POWER(Shutdown,    "shutdown")     \
    POWER(Battery,     "battery")      \
    POWER(IdleSleep,   "idle_sleep")   \
    POWER(IdleDisplay, "idle_display") \
    POWER(IdleMute,    "idle_mute")    \
    POWER(GovIdle,     "gov_idle")     \
    POWER(GovDefault,  "gov_default")

#define RTC_ELEMENTS          \
    RTC(Timezone, "timezone") \
    RTC(Year,     "year")     \
    RTC(Month,    "month")    \
    RTC(Day,      "day")      \
    RTC(Hour,     "hour")     \
    RTC(Minute,   "minute")   \
    RTC(Notation, "notation")

#define SEARCH_ELEMENTS            \
    SEARCH(Lookup,       "lookup") \
    SEARCH(SearchLocal,  "local")  \
    SEARCH(SearchGlobal, "global")

#define SPACE_ELEMENTS            \
    SPACE(Primary,   "primary")   \
    SPACE(Secondary, "secondary") \
    SPACE(External,  "external")  \
    SPACE(System,    "system")

#define STORAGE_ELEMENTS              \
    STORAGE(Apps,       "apps")       \
    STORAGE(Bios,       "bios")       \
    STORAGE(Catalogue,  "catalogue")  \
    STORAGE(Collection, "collection") \
    STORAGE(History,    "history")    \
    STORAGE(Init,       "init")       \
    STORAGE(Music,      "music")      \
    STORAGE(Name,       "name")       \
    STORAGE(Network,    "network")    \
    STORAGE(Package,    "package")    \
    STORAGE(Save,       "save")       \
    STORAGE(Screenshot, "screenshot") \
    STORAGE(Syncthing,  "syncthing")  \
    STORAGE(Theme,      "theme")      \
    STORAGE(Track,      "track")

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
    SYSINFO(Voltage,  "voltage")  \
    SYSINFO(Refresh,  "refresh")

#define TWEAKADV_ELEMENTS                \
    TWEAKADV(Accelerate,  "accelerate")  \
    TWEAKADV(RepeatDelay, "repeat")      \
    TWEAKADV(Offset,      "offset")      \
    TWEAKADV(Swap,        "swap")        \
    TWEAKADV(Volume,      "volume")      \
    TWEAKADV(Brightness,  "brightness")  \
    TWEAKADV(Thermal,     "thermal")     \
    TWEAKADV(Passcode,    "lock")        \
    TWEAKADV(Led,         "led")         \
    TWEAKADV(Theme,       "theme")       \
    TWEAKADV(RetroWait,   "retrowait")   \
    TWEAKADV(RetroFree,   "retrofree")   \
    TWEAKADV(Verbose,     "verbose")     \
    TWEAKADV(Rumble,      "rumble")      \
    TWEAKADV(UserInit,    "userinit")    \
    TWEAKADV(DpadSwap,    "dpadswap")    \
    TWEAKADV(Overdrive,   "overdrive")   \
    TWEAKADV(LidSwitch,   "lidswitch")   \
    TWEAKADV(DispSuspend, "dispsuspend") \
    TWEAKADV(Swapfile,    "swapfile")    \
    TWEAKADV(Zramfile,    "zramfile")    \
    TWEAKADV(SecondPart,  "secondpart")  \
    TWEAKADV(UsbPart,     "usbpart")     \
    TWEAKADV(IncBright,   "incbright")   \
    TWEAKADV(IncVolume,   "invvolume")

#define TWEAKGEN_ELEMENTS              \
    TWEAKGEN(Rtc,        "clock")      \
    TWEAKGEN(Hdmi,       "hdmi")       \
    TWEAKGEN(Advanced,   "advanced")   \
    TWEAKGEN(Brightness, "brightness") \
    TWEAKGEN(Volume,     "volume")     \
    TWEAKGEN(Colour,     "colour")     \
    TWEAKGEN(Rgb,        "rgb")        \
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
    WEBSERV(Ntp,        "ntp")        \
    WEBSERV(Tailscaled, "tailscaled")
