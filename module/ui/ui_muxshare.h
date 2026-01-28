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

#define APPCON_ELEMENTS                    \
    APPCON(Governor, GOVERNOR, "governor") \
    APPCON(Control,  CONTROL,  "control")

#define BACKUP_ELEMENTS                           \
    BACKUP(Track,      TRACK,      "track")       \
    BACKUP(Apps,       APPS,       "application") \
    BACKUP(Music,      MUSIC,      "music")       \
    BACKUP(Collection, COLLECTION, "collection")  \
    BACKUP(Override,   OVERRIDE,   "override")    \
    BACKUP(Package,    PACKAGE,    "package")     \
    BACKUP(Name,       NAME,       "name")        \
    BACKUP(History,    HISTORY,    "history")     \
    BACKUP(Catalogue,  CATALOGUE,  "catalogue")   \
    BACKUP(Network,    NETWORK,    "network")     \
    BACKUP(Cheats,     CHEATS,     "cheats")      \
    BACKUP(Config,     CONFIG,     "config")      \
    BACKUP(Overlays,   OVERLAYS,   "overlays")    \
    BACKUP(Shaders,    SHADERS,    "shaders")     \
    BACKUP(Save,       SAVE,       "save")        \
    BACKUP(Screenshot, SCREENSHOT, "screenshot")  \
    BACKUP(Syncthing,  SYNCTHING,  "syncthing")   \
    BACKUP(Bios,       BIOS,       "bios")        \
    BACKUP(Theme,      THEME,      "theme")       \
    BACKUP(Init,       INIT,       "init")        \
    BACKUP(Target,     TARGET,     "target")      \
    BACKUP(Merge,      MERGE,      "merge")       \
    BACKUP(Start,      START,      "start")

#define CHRONY_ELEMENTS                         \
    CHRONY(Reference,  REFERENCE,  "reference") \
    CHRONY(Stratum,    STRATUM,    "stratum")   \
    CHRONY(RefTime,    REFTIME,    "reftime")   \
    CHRONY(SystemTime, SYSTEMTIME, "system")    \
    CHRONY(LastOffset, LASTOFFSET, "last")      \
    CHRONY(RmsOffset,  RMSOFFSET,  "rms")       \
    CHRONY(Frequency,  FREQUENCY,  "freq")      \
    CHRONY(RootDelay,  ROOTDELAY,  "delay")     \
    CHRONY(RootDisp,   ROOTDISP,   "disp")      \
    CHRONY(UpdateInt,  UPDATEINT,  "update")    \
    CHRONY(Leap,       LEAP,       "leap")

#define COLADJUST_ELEMENTS                             \
    COLADJUST(Temperature, TEMPERATURE, "temperature") \
    COLADJUST(Brightness,  BRIGHTNESS,  "brightness")  \
    COLADJUST(Contrast,    CONTRAST,    "contrast")    \
    COLADJUST(Saturation,  SATURATION,  "saturation")  \
    COLADJUST(HueShift,    HUESHIFT,    "hueshift")    \
    COLADJUST(Gamma,       GAMMA,       "gamma")

#define CONFIG_ELEMENTS                       \
    CONFIG(General,   GENERAL,   "general")   \
    CONFIG(Connect,   CONNECT,   "connect")   \
    CONFIG(Custom,    CUSTOM,    "custom")    \
    CONFIG(Interface, INTERFACE, "interface") \
    CONFIG(Colour,    COLOUR,    "colour")    \
    CONFIG(Overlay,   OVERLAY,   "overlay")   \
    CONFIG(Language,  LANGUAGE,  "language")  \
    CONFIG(Power,     POWER,     "power")     \
    CONFIG(Storage,   STORAGE,   "storage")   \
    CONFIG(Backup,    BACKUP,    "backup")

#define CONNECT_ELEMENTS                             \
    CONNECT(Network,     NETWORK,     "network")     \
    CONNECT(NetAdv,      NETADV,      "netadv")      \
    CONNECT(Services,    SERVICES,    "service")     \
    CONNECT(Bluetooth,   BLUETOOTH,   "bluetooth")   \
    CONNECT(UsbFunction, USBFUNCTION, "usbfunction")

#define CUSTOM_ELEMENTS                                         \
    CUSTOM(Catalogue,       CATALOGUE,       "catalogue")       \
    CUSTOM(Config,          CONFIG,          "config")          \
    CUSTOM(Theme,           THEME,           "theme")           \
    CUSTOM(ThemeResolution, THEMERESOLUTION, "resolution")      \
    CUSTOM(ThemeScaling,    THEMESCALING,    "scaling")         \
    CUSTOM(ThemeAlternate,  THEMEALTERNATE,  "alternate")       \
    CUSTOM(Animation,       ANIMATION,       "animation")       \
    CUSTOM(Music,           MUSIC,           "music")           \
    CUSTOM(MusicVolume,     MUSICVOLUME,     "musicvolume")     \
    CUSTOM(BlackFade,       BLACKFADE,       "blackfade")       \
    CUSTOM(LaunchSwap,      LAUNCHSWAP,      "launch_swap")     \
    CUSTOM(Shuffle,         SHUFFLE,         "shuffle")         \
    CUSTOM(BoxArtImage,     BOXARTIMAGE,     "boxart")          \
    CUSTOM(BoxArtAlign,     BOXARTALIGN,     "align")           \
    CUSTOM(ContentWidth,    CONTENTWIDTH,    "width")           \
    CUSTOM(LaunchSplash,    LAUNCHSPLASH,    "splash")          \
    CUSTOM(GridModeContent, GRIDMODECONTENT, "gridmodecontent") \
    CUSTOM(BoxArtHide,      BOXARTHIDE,      "boxarthide")      \
    CUSTOM(Font,            FONT,            "font")            \
    CUSTOM(Sound,           SOUND,           "sound")           \
    CUSTOM(SoundVolume,     SOUNDVOLUME,     "soundvolume")     \
    CUSTOM(Chime,           CHIME,           "chime")

#define DANGER_ELEMENTS                                 \
    DANGER(VmSwap,        VMSWAP,        "vmswap")      \
    DANGER(DirtyRatio,    DIRTYRATIO,    "dirty-ratio") \
    DANGER(DirtyBack,     DIRTYBACK,     "dirty-back")  \
    DANGER(CachePressure, CACHEPRESSURE, "cache")       \
    DANGER(NoMerge,       NOMERGE,       "merge")       \
    DANGER(NrRequests,    NRREQUESTS,    "requests")    \
    DANGER(ReadAhead,     READAHEAD,     "readahead")   \
    DANGER(PageCluster,   PAGECLUSTER,   "cluster")     \
    DANGER(TimeSlice,     TIMESLICE,     "timeslice")   \
    DANGER(IoStats,       IOSTATS,       "iostats")     \
    DANGER(IdleFlush,     IDLEFLUSH,     "idleflush")   \
    DANGER(ChildFirst,    CHILDFIRST,    "child")       \
    DANGER(TuneScale,     TUNESCALE,     "tunescale")   \
    DANGER(CardMode,      CARDMODE,      "cardmode")    \
    DANGER(State,         STATE,         "state")

#define DEVICE_ELEMENTS                                \
    DEVICE(HasBluetooth,  HASBLUETOOTH,  "bluetooth")  \
    DEVICE(HasRgb,        HASRGB,        "rgb")        \
    DEVICE(HasDebugFs,    HASDEBUGFS,    "debugfs")    \
    DEVICE(HasHdmi,       HASHDMI,       "hdmi")       \
    DEVICE(HasLid,        HASLID,        "lid")        \
    DEVICE(HasNetwork,    HASNETWORK,    "network")    \
    DEVICE(HasPortmaster, HASPORTMASTER, "portmaster")

#define HDMI_ELEMENTS                          \
    HDMI(Resolution, RESOLUTION, "resolution") \
    HDMI(Space,      SPACE,      "space")      \
    HDMI(Depth,      DEPTH,      "depth")      \
    HDMI(Range,      RANGE,      "range")      \
    HDMI(Scan,       SCAN,       "scan")       \
    HDMI(Audio,      AUDIO,      "audio")

#define INFO_ELEMENTS                          \
    INFO(News,       NEWS,       "news")       \
    INFO(Activity,   ACTIVITY,   "activity")   \
    INFO(Screenshot, SCREENSHOT, "screenshot") \
    INFO(Space,      SPACE,      "space")      \
    INFO(Tester,     TESTER,     "tester")     \
    INFO(SysInfo,    SYSINFO,    "sysinfo")    \
    INFO(NetInfo,    NETINFO,    "netinfo")    \
    INFO(Chrony,     CHRONY,     "chrony")     \
    INFO(Credit,     CREDIT,     "credit")

#define INSTALL_ELEMENTS                    \
    INSTALL(Rtc,      RTC,      "clock")    \
    INSTALL(Language, LANGUAGE, "language") \
    INSTALL(Shutdown, SHUTDOWN, "shutdown") \
    INSTALL(Install,  INSTALL,  "install")

#define KIOSK_ELEMENTS                          \
    KIOSK(Enable,     ENABLE,     "enable")     \
    KIOSK(Message,    MESSAGE,    "message")    \
    KIOSK(Archive,    ARCHIVE,    "archive")    \
    KIOSK(Task,       TASK,       "task")       \
    KIOSK(Custom,     CUSTOM,     "custom")     \
    KIOSK(Language,   LANGUAGE,   "language")   \
    KIOSK(Network,    NETWORK,    "network")    \
    KIOSK(Storage,    STORAGE,    "storage")    \
    KIOSK(Backup,     BACKUP,     "backup")     \
    KIOSK(NetAdv,     NETADV,     "netadv")     \
    KIOSK(WebServ,    WEBSERV,    "webserv")    \
    KIOSK(Core,       CORE,       "core")       \
    KIOSK(Governor,   GOVERNOR,   "governor")   \
    KIOSK(Control,    CONTROL,    "control")    \
    KIOSK(Option,     OPTION,     "option")     \
    KIOSK(RetroArch,  RETROARCH,  "retroarch")  \
    KIOSK(Search,     SEARCH,     "search")     \
    KIOSK(Tag,        TAG,        "tag")        \
    KIOSK(ColFilter,  COLFILTER,  "colfilter")  \
    KIOSK(Catalogue,  CATALOGUE,  "catalogue")  \
    KIOSK(RAConfig,   RACONFIG,   "raconfig")   \
    KIOSK(Theme,      THEME,      "theme")      \
    KIOSK(ThemeDown,  THEMEDOWN,  "theme_down") \
    KIOSK(Clock,      CLOCK,      "clock")      \
    KIOSK(Timezone,   TIMEZONE,   "timezone")   \
    KIOSK(Apps,       APPS,       "apps")       \
    KIOSK(Config,     CONFIG,     "config")     \
    KIOSK(Explore,    EXPLORE,    "explore")    \
    KIOSK(CollectMod, COLLECTMOD, "collectmod") \
    KIOSK(CollectAdd, COLLECTADD, "collectadd") \
    KIOSK(CollectNew, COLLECTNEW, "collectnew") \
    KIOSK(CollectRem, COLLECTREM, "collectrem") \
    KIOSK(CollectAcc, COLLECTACC, "collectacc") \
    KIOSK(HistoryMod, HISTORYMOD, "historymod") \
    KIOSK(HistoryRem, HISTORYREM, "historyrem") \
    KIOSK(Info,       INFO,       "info")       \
    KIOSK(Advanced,   ADVANCED,   "advanced")   \
    KIOSK(General,    GENERAL,    "general")    \
    KIOSK(Hdmi,       HDMI,       "hdmi")       \
    KIOSK(Power,      POWER,      "power")      \
    KIOSK(Visual,     VISUAL,     "visual")     \
    KIOSK(Overlay,    OVERLAY,    "overlay")

#define LAUNCH_ELEMENTS                          \
    LAUNCH(Explore,    EXPLORE,    "explore")    \
    LAUNCH(Collection, COLLECTION, "collection") \
    LAUNCH(History,    HISTORY,    "history")    \
    LAUNCH(Apps,       APPS,       "apps")       \
    LAUNCH(Info,       INFO,       "info")       \
    LAUNCH(Config,     CONFIG,     "config")     \
    LAUNCH(Reboot,     REBOOT,     "reboot")     \
    LAUNCH(Shutdown,   SHUTDOWN,   "shutdown")

#define NETADV_ELEMENTS                       \
    NETADV(Monitor,   MONITOR,   "monitor")   \
    NETADV(Boot,      BOOT,      "boot")      \
    NETADV(Wake,      WAKE,      "wake")      \
    NETADV(Compat,    COMPAT,    "compat")    \
    NETADV(AsyncLoad, ASYNCLOAD, "asyncload") \
    NETADV(ConRetry,  CONRETRY,  "conretry")  \
    NETADV(Wait,      WAIT,      "wait")      \
    NETADV(ModRetry,  MODRETRY,  "modretry")

#define NETINFO_ELEMENTS                       \
    NETINFO(Hostname,  HOSTNAME,  "hostname")  \
    NETINFO(Mac,       MAC,       "mac")       \
    NETINFO(Ip,        IP,        "ip")        \
    NETINFO(Ssid,      SSID,      "ssid")      \
    NETINFO(Gateway,   GATEWAY,   "gateway")   \
    NETINFO(Dns,       DNS,       "dns")       \
    NETINFO(Signal,    SIGNAL,    "signal")    \
    NETINFO(Channel,   CHANNEL,   "channel")   \
    NETINFO(AcTraffic, ACTRAFFIC, "actraffic") \
    NETINFO(TpTraffic, TPTRAFFIC, "tptraffic")

#define NETWORK_ELEMENTS                          \
    NETWORK(Identifier, IDENTIFIER, "identifier") \
    NETWORK(Password,   PASSWORD,   "password")   \
    NETWORK(Hidden,     HIDDEN,     "hidden")     \
    NETWORK(Type,       TYPE,       "type")       \
    NETWORK(Address,    ADDRESS,    "address")    \
    NETWORK(Subnet,     SUBNET,     "subnet")     \
    NETWORK(Gateway,    GATEWAY,    "gateway")    \
    NETWORK(Dns,        DNS,        "dns")        \
    NETWORK(Connect,    CONNECT,    "connect")

#define OPTION_ELEMENTS                       \
    OPTION(Search,    SEARCH,    "search")    \
    OPTION(Core,      CORE,      "core")      \
    OPTION(Governor,  GOVERNOR,  "governor")  \
    OPTION(Control,   CONTROL,   "control")   \
    OPTION(RetroArch, RETROARCH, "retroarch") \
    OPTION(ColFilter, COLFILTER, "colfilter") \
    OPTION(Tag,       TAG,       "tag")

#define OVERLAY_ELEMENTS                        \
    OVERLAY(GenAlpha,  GENALPHA,  "gen_alpha")  \
    OVERLAY(GenAnchor, GENANCHOR, "gen_anchor") \
    OVERLAY(GenScale,  GENSCALE,  "gen_scale")  \
    OVERLAY(BatAlpha,  BATALPHA,  "bat_alpha")  \
    OVERLAY(BatAnchor, BATANCHOR, "bat_anchor") \
    OVERLAY(BatScale,  BATSCALE,  "bat_scale")  \
    OVERLAY(VolAlpha,  VOLALPHA,  "vol_alpha")  \
    OVERLAY(VolAnchor, VOLANCHOR, "vol_anchor") \
    OVERLAY(VolScale,  VOLSCALE,  "vol_scale")  \
    OVERLAY(BriAlpha,  BRIALPHA,  "bri_alpha")  \
    OVERLAY(BriAnchor, BRIANCHOR, "bri_anchor") \
    OVERLAY(BriScale,  BRISCALE,  "bri_scale")

#define POWER_ELEMENTS                              \
    POWER(Shutdown,    SHUTDOWN,    "shutdown")     \
    POWER(Battery,     BATTERY,     "battery")      \
    POWER(IdleSleep,   IDLESLEEP,   "idle_sleep")   \
    POWER(IdleDisplay, IDLEDISPLAY, "idle_display") \
    POWER(IdleMute,    IDLEMUTE,    "idle_mute")    \
    POWER(GovIdle,     GOVIDLE,     "gov_idle")     \
    POWER(GovDefault,  GOVDEFAULT,  "gov_default")  \
    POWER(Screensaver, SCREENSAVER, "screensaver")

#define RTC_ELEMENTS                    \
    RTC(Timezone, TIMEZONE, "timezone") \
    RTC(Year,     YEAR,     "year")     \
    RTC(Month,    MONTH,    "month")    \
    RTC(Day,      DAY,      "day")      \
    RTC(Hour,     HOUR,     "hour")     \
    RTC(Minute,   MINUTE,   "minute")   \
    RTC(Notation, NOTATION, "notation")

#define SEARCH_ELEMENTS                          \
    SEARCH(Lookup,       LOOKUP,       "lookup") \
    SEARCH(SearchLocal,  SEARCHLOCAL,  "local")  \
    SEARCH(SearchGlobal, SEARCHGLOBAL, "global")

#define SPACE_ELEMENTS                       \
    SPACE(Primary,   PRIMARY,   "primary")   \
    SPACE(Secondary, SECONDARY, "secondary") \
    SPACE(External,  EXTERNAL,  "external")  \
    SPACE(System,    SYSTEM,    "system")

#define STORAGE_ELEMENTS                          \
    STORAGE(Apps,       APPS,       "apps")       \
    STORAGE(Bios,       BIOS,       "bios")       \
    STORAGE(Catalogue,  CATALOGUE,  "catalogue")  \
    STORAGE(Collection, COLLECTION, "collection") \
    STORAGE(History,    HISTORY,    "history")    \
    STORAGE(Init,       INIT,       "init")       \
    STORAGE(Music,      MUSIC,      "music")      \
    STORAGE(Name,       NAME,       "name")       \
    STORAGE(Network,    NETWORK,    "network")    \
    STORAGE(Package,    PACKAGE,    "package")    \
    STORAGE(Save,       SAVE,       "save")       \
    STORAGE(Screenshot, SCREENSHOT, "screenshot") \
    STORAGE(Syncthing,  SYNCTHING,  "syncthing")  \
    STORAGE(Theme,      THEME,      "theme")      \
    STORAGE(Track,      TRACK,      "track")

#define SYSINFO_ELEMENTS                    \
    SYSINFO(Version,  VERSION,  "version")  \
    SYSINFO(Build,    BUILD,    "build")    \
    SYSINFO(Device,   DEVICE,   "device")   \
    SYSINFO(Kernel,   KERNEL,   "kernel")   \
    SYSINFO(Uptime,   UPTIME,   "uptime")   \
    SYSINFO(Cpu,      CPU,      "cpu")      \
    SYSINFO(Speed,    SPEED,    "speed")    \
    SYSINFO(Governor, GOVERNOR, "governor") \
    SYSINFO(Memory,   MEMORY,   "memory")   \
    SYSINFO(Temp,     TEMP,     "temp")     \
    SYSINFO(Capacity, CAPACITY, "capacity") \
    SYSINFO(Voltage,  VOLTAGE,  "voltage")  \
    SYSINFO(Charger,  CHARGER,  "charger")  \
    SYSINFO(Reload,   RELOAD,   "reload")

#define THEMEFILTER_ELEMENTS                      \
    THEMEFILTER(AllThemes, ALLTHEMES, "theme")    \
    THEMEFILTER(Grid,      GRID,      "grid")     \
    THEMEFILTER(Hdmi,      HDMI,      "hdmi")     \
    THEMEFILTER(Language,  LANGUAGE,  "language")

#define TWEAKADV_ELEMENTS                             \
    TWEAKADV(Accelerate,  ACCELERATE,  "accelerate")  \
    TWEAKADV(RepeatDelay, REPEATDELAY, "repeat")      \
    TWEAKADV(Offset,      OFFSET,      "offset")      \
    TWEAKADV(Swap,        SWAP,        "swap")        \
    TWEAKADV(StickNav,    STICKNAV,    "sticknav")    \
    TWEAKADV(Volume,      VOLUME,      "volume")      \
    TWEAKADV(Brightness,  BRIGHTNESS,  "brightness")  \
    TWEAKADV(Thermal,     THERMAL,     "thermal")     \
    TWEAKADV(Passcode,    PASSCODE,    "lock")        \
    TWEAKADV(Led,         LED,         "led")         \
    TWEAKADV(RandomTheme, RANDOMTHEME, "randomtheme") \
    TWEAKADV(RetroWait,   RETROWAIT,   "retrowait")   \
    TWEAKADV(RetroFree,   RETROFREE,   "retrofree")   \
    TWEAKADV(RetroCache,  RETROCACHE,  "retrocache")  \
    TWEAKADV(Activity,    ACTIVITY,    "activity")    \
    TWEAKADV(Verbose,     VERBOSE,     "verbose")     \
    TWEAKADV(Rumble,      RUMBLE,      "rumble")      \
    TWEAKADV(UserInit,    USERINIT,    "userinit")    \
    TWEAKADV(DpadSwap,    DPADSWAP,    "dpadswap")    \
    TWEAKADV(Overdrive,   OVERDRIVE,   "overdrive")   \
    TWEAKADV(LidSwitch,   LIDSWITCH,   "lidswitch")   \
    TWEAKADV(DispSuspend, DISPSUSPEND, "dispsuspend") \
    TWEAKADV(Swapfile,    SWAPFILE,    "swapfile")    \
    TWEAKADV(Zramfile,    ZRAMFILE,    "zramfile")    \
    TWEAKADV(SecondPart,  SECONDPART,  "secondpart")  \
    TWEAKADV(UsbPart,     USBPART,     "usbpart")     \
    TWEAKADV(IncBright,   INCBRIGHT,   "incbright")   \
    TWEAKADV(IncVolume,   INCVOLUME,   "invvolume")   \
    TWEAKADV(MaxGpu,      MAXGPU,      "maxgpu")      \
    TWEAKADV(AudioReady,  AUDIOREADY,  "audioready")  \
    TWEAKADV(AudioSwap,   AUDIOSWAP,   "audioswap")

#define TWEAKGEN_ELEMENTS                          \
    TWEAKGEN(Rtc,        RTC,        "clock")      \
    TWEAKGEN(Hdmi,       HDMI,       "hdmi")       \
    TWEAKGEN(Advanced,   ADVANCED,   "advanced")   \
    TWEAKGEN(Brightness, BRIGHTNESS, "brightness") \
    TWEAKGEN(Volume,     VOLUME,     "volume")     \
    TWEAKGEN(Rgb,        RGB,        "rgb")        \
    TWEAKGEN(HkDpad,     HKDPAD,     "hkdpad")     \
    TWEAKGEN(HkShot,     HKSHOT,     "hkshot")     \
    TWEAKGEN(Startup,    STARTUP,    "startup")

#define VISUAL_ELEMENTS                                                           \
    VISUAL(Battery,               BATTERY,               "battery")               \
    VISUAL(Clock,                 CLOCK,                 "clock")                 \
    VISUAL(Network,               NETWORK,               "network")               \
    VISUAL(Name,                  NAME,                  "name")                  \
    VISUAL(Dash,                  DASH,                  "dash")                  \
    VISUAL(FriendlyFolder,        FRIENDLYFOLDER,        "friendlyfolder")        \
    VISUAL(TheTitleFormat,        THETITLEFORMAT,        "thetitleformat")        \
    VISUAL(TitleIncludeRootDrive, TITLEINCLUDEROOTDRIVE, "titleincluderootdrive") \
    VISUAL(FolderItemCount,       FOLDERITEMCOUNT,       "folderitemcount")       \
    VISUAL(DisplayEmptyFolder,    DISPLAYEMPTYFOLDER,    "folderempty")           \
    VISUAL(MenuCounterFolder,     MENUCOUNTERFOLDER,     "counterfolder")         \
    VISUAL(MenuCounterFile,       MENUCOUNTERFILE,       "counterfile")           \
    VISUAL(Hidden,                HIDDEN,                "hidden")                \
    VISUAL(ContentCollect,        CONTENTCOLLECT,        "contentcollect")        \
    VISUAL(ContentHistory,        CONTENTHISTORY,        "contenthistory")        \
    VISUAL(OverlayImage,          OVERLAYIMAGE,          "overlayimage")          \
    VISUAL(OverlayTransparency,   OVERLAYTRANSPARENCY,   "overlaytransparency")

#define WEBSERV_ELEMENTS                          \
    WEBSERV(Sshd,       SSHD,       "sshd")       \
    WEBSERV(SftpGo,     SFTPGO,     "sftpgo")     \
    WEBSERV(Ttyd,       TTYD,       "ttyd")       \
    WEBSERV(Syncthing,  SYNCTHING,  "syncthing")  \
    WEBSERV(Tailscaled, TAILSCALED, "tailscaled")
