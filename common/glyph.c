#include <stdio.h>
#include "common.h"
#include "options.h"
#include "glyph.h"
#include "mini/mini.h"

void load_glyph(struct glyph_config *glyph, char *mux_name) {
    char scheme[MAX_BUFFER_SIZE];
    snprintf(scheme, sizeof(scheme), "%s/%s.txt", MUOS_GLYPH_DIR, mux_name);
    if (!file_exist(scheme)) {
        snprintf(scheme, sizeof(scheme), "%s/default.txt", MUOS_GLYPH_DIR);
    }

    mini_t * muos_glyph = mini_try_load(scheme);

    glyph->STATUS.BLUETOOTH.ACTIVE = get_ini_unicode(muos_glyph, "status.bluetooth", "ACTIVE");
    glyph->STATUS.BLUETOOTH.DISABLED = get_ini_unicode(muos_glyph, "status.bluetooth", "DISABLED");

    glyph->STATUS.NETWORK.ACTIVE = get_ini_unicode(muos_glyph, "status.network", "ACTIVE");
    glyph->STATUS.NETWORK.DISABLED = get_ini_unicode(muos_glyph, "status.network", "DISABLED");

    glyph->STATUS.BATTERY.ACTIVE = get_ini_unicode(muos_glyph, "status.battery", "ACTIVE");
    glyph->STATUS.BATTERY.EMPTY = get_ini_unicode(muos_glyph, "status.battery", "EMPTY");
    glyph->STATUS.BATTERY.LOW = get_ini_unicode(muos_glyph, "status.battery", "LOW");
    glyph->STATUS.BATTERY.MEDIUM = get_ini_unicode(muos_glyph, "status.battery", "MEDIUM");
    glyph->STATUS.BATTERY.HIGH = get_ini_unicode(muos_glyph, "status.battery", "HIGH");
    glyph->STATUS.BATTERY.FULL = get_ini_unicode(muos_glyph, "status.battery", "FULL");

    glyph->NAV.A = get_ini_unicode(muos_glyph, "navigation", "A");
    glyph->NAV.B = get_ini_unicode(muos_glyph, "navigation", "B");
    glyph->NAV.C = get_ini_unicode(muos_glyph, "navigation", "C");
    glyph->NAV.X = get_ini_unicode(muos_glyph, "navigation", "X");
    glyph->NAV.Y = get_ini_unicode(muos_glyph, "navigation", "Y");
    glyph->NAV.Z = get_ini_unicode(muos_glyph, "navigation", "Z");
    glyph->NAV.MENU = get_ini_unicode(muos_glyph, "navigation", "MENU");

    glyph->PROGRESS.BRIGHTNESS = get_ini_unicode(muos_glyph, "progress", "BRIGHTNESS");
    glyph->PROGRESS.VOLUME_MUTE = get_ini_unicode(muos_glyph, "progress", "VOLUME_MUTE");
    glyph->PROGRESS.VOLUME_LOW = get_ini_unicode(muos_glyph, "progress", "VOLUME_LOW");
    glyph->PROGRESS.VOLUME_MEDIUM = get_ini_unicode(muos_glyph, "progress", "VOLUME_MEDIUM");
    glyph->PROGRESS.VOLUME_HIGH = get_ini_unicode(muos_glyph, "progress", "VOLUME_HIGH");

    glyph->MUX_APPS.ARCHIVE_MANAGER = get_ini_unicode(muos_glyph, "muxapps", "ARCHIVE_MANAGER");
    glyph->MUX_APPS.BACKUP_MANAGER = get_ini_unicode(muos_glyph, "muxapps", "BACKUP_MANAGER");
    glyph->MUX_APPS.PORTMASTER = get_ini_unicode(muos_glyph, "muxapps", "PORTMASTER");
    glyph->MUX_APPS.RETROARCH = get_ini_unicode(muos_glyph, "muxapps", "RETROARCH");
    glyph->MUX_APPS.DINGUX = get_ini_unicode(muos_glyph, "muxapps", "DINGUX");
    glyph->MUX_APPS.GMU = get_ini_unicode(muos_glyph, "muxapps", "GMU");

    glyph->MUX_ARCHIVE.ITEM = get_ini_unicode(muos_glyph, "muxarchive", "ITEM");

    glyph->MUX_ASSIGN.SYSTEM = get_ini_unicode(muos_glyph, "muxassign", "SYSTEM");
    glyph->MUX_ASSIGN.CORE = get_ini_unicode(muos_glyph, "muxassign", "CORE");

    glyph->MUX_BACKUP.ITEM = get_ini_unicode(muos_glyph, "muxbackup", "ITEM");

    glyph->MUX_CONFIG.GENERAL_SETTINGS = get_ini_unicode(muos_glyph, "muxconfig", "GENERAL_SETTINGS");
    glyph->MUX_CONFIG.THEME_PICKER = get_ini_unicode(muos_glyph, "muxconfig", "THEME_PICKER");
    glyph->MUX_CONFIG.WIFI_NETWORK = get_ini_unicode(muos_glyph, "muxconfig", "WIFI_NETWORK");
    glyph->MUX_CONFIG.WEB_SERVICES = get_ini_unicode(muos_glyph, "muxconfig", "WEB_SERVICES");
    glyph->MUX_CONFIG.DATE_TIME = get_ini_unicode(muos_glyph, "muxconfig", "DATE_TIME");
    glyph->MUX_CONFIG.DEVICE_TYPE = get_ini_unicode(muos_glyph, "muxconfig", "DEVICE_TYPE");
    glyph->MUX_CONFIG.SYSTEM_REFRESH = get_ini_unicode(muos_glyph, "muxconfig", "SYSTEM_REFRESH");

    glyph->MUX_CREDITS.XONGLEBONGLE = get_ini_unicode(muos_glyph, "muxcredits", "XONGLEBONGLE");
    glyph->MUX_CREDITS.SUPPORTER = get_ini_unicode(muos_glyph, "muxcredits", "SUPPORTER");

    glyph->MUX_DEVICE.ITEM = get_ini_unicode(muos_glyph, "muxdevice", "ITEM");

    glyph->MUX_INFO.INPUT_TESTER = get_ini_unicode(muos_glyph, "muxinfo", "INPUT_TESTER");
    glyph->MUX_INFO.SYSTEM_DETAILS = get_ini_unicode(muos_glyph, "muxinfo", "SYSTEM_DETAILS");
    glyph->MUX_INFO.SUPPORTERS = get_ini_unicode(muos_glyph, "muxinfo", "SUPPORTERS");

    glyph->MUX_LAUNCH.EXPLORE = get_ini_unicode(muos_glyph, "muxlaunch", "EXPLORE");
    glyph->MUX_LAUNCH.FAVOURITES = get_ini_unicode(muos_glyph, "muxlaunch", "FAVOURITES");
    glyph->MUX_LAUNCH.HISTORY = get_ini_unicode(muos_glyph, "muxlaunch", "HISTORY");
    glyph->MUX_LAUNCH.APPS = get_ini_unicode(muos_glyph, "muxlaunch", "APPS");
    glyph->MUX_LAUNCH.INFO = get_ini_unicode(muos_glyph, "muxlaunch", "INFO");
    glyph->MUX_LAUNCH.CONFIG = get_ini_unicode(muos_glyph, "muxlaunch", "CONFIG");
    glyph->MUX_LAUNCH.REBOOT = get_ini_unicode(muos_glyph, "muxlaunch", "REBOOT");
    glyph->MUX_LAUNCH.SHUTDOWN = get_ini_unicode(muos_glyph, "muxlaunch", "SHUTDOWN");

    glyph->MUX_NETSCAN.ITEM = get_ini_unicode(muos_glyph, "muxnetscan", "ITEM");

    glyph->MUX_NETWORK.ENABLE = get_ini_unicode(muos_glyph, "muxnetwork", "ENABLE");
    glyph->MUX_NETWORK.SSID = get_ini_unicode(muos_glyph, "muxnetwork", "SSID");
    glyph->MUX_NETWORK.PASS = get_ini_unicode(muos_glyph, "muxnetwork", "PASS");
    glyph->MUX_NETWORK.TYPE = get_ini_unicode(muos_glyph, "muxnetwork", "TYPE");
    glyph->MUX_NETWORK.IP = get_ini_unicode(muos_glyph, "muxnetwork", "IP");
    glyph->MUX_NETWORK.CIDR = get_ini_unicode(muos_glyph, "muxnetwork", "CIDR");
    glyph->MUX_NETWORK.GATEWAY = get_ini_unicode(muos_glyph, "muxnetwork", "GATEWAY");
    glyph->MUX_NETWORK.DNS = get_ini_unicode(muos_glyph, "muxnetwork", "DNS");
    glyph->MUX_NETWORK.STATUS = get_ini_unicode(muos_glyph, "muxnetwork", "STATUS");
    glyph->MUX_NETWORK.CONNECT = get_ini_unicode(muos_glyph, "muxnetwork", "CONNECT");

    glyph->MUX_EXPLORE.DIRECTORY = get_ini_unicode(muos_glyph, "muxplore", "DIRECTORY");
    glyph->MUX_EXPLORE.FILE = get_ini_unicode(muos_glyph, "muxplore", "FILE");
    glyph->MUX_EXPLORE.STORAGE = get_ini_unicode(muos_glyph, "muxplore", "STORAGE");

    glyph->MUX_RESET.CLEAR_FAVOURITES = get_ini_unicode(muos_glyph, "muxreset", "CLEAR_FAVOURITES");
    glyph->MUX_RESET.CLEAR_HISTORY = get_ini_unicode(muos_glyph, "muxreset", "CLEAR_HISTORY");
    glyph->MUX_RESET.CLEAR_ACTIVITY = get_ini_unicode(muos_glyph, "muxreset", "CLEAR_ACTIVITY");
    glyph->MUX_RESET.CLEAR_SYSTEM_CONFIG = get_ini_unicode(muos_glyph, "muxreset", "CLEAR_SYSTEM_CONFIG");
    glyph->MUX_RESET.CLEAR_SYSTEM_CACHE = get_ini_unicode(muos_glyph, "muxreset", "CLEAR_SYSTEM_CACHE");
    glyph->MUX_RESET.RESTORE_RETROARCH_CONFIG = get_ini_unicode(muos_glyph, "muxreset", "RESTORE_RETROARCH_CONFIG");
    glyph->MUX_RESET.RESTORE_NETWORK_CONFIG = get_ini_unicode(muos_glyph, "muxreset", "RESTORE_NETWORK_CONFIG");
    glyph->MUX_RESET.RESTORE_PORTMASTER = get_ini_unicode(muos_glyph, "muxreset", "RESTORE_PORTMASTER");

    glyph->MUX_RTC.YEAR = get_ini_unicode(muos_glyph, "muxrtc", "YEAR");
    glyph->MUX_RTC.MONTH = get_ini_unicode(muos_glyph, "muxrtc", "MONTH");
    glyph->MUX_RTC.DAY = get_ini_unicode(muos_glyph, "muxrtc", "DAY");
    glyph->MUX_RTC.HOUR = get_ini_unicode(muos_glyph, "muxrtc", "HOUR");
    glyph->MUX_RTC.MINUTE = get_ini_unicode(muos_glyph, "muxrtc", "MINUTE");
    glyph->MUX_RTC.NOTATION = get_ini_unicode(muos_glyph, "muxrtc", "NOTATION");
    glyph->MUX_RTC.TIMEZONE = get_ini_unicode(muos_glyph, "muxrtc", "TIMEZONE");

    glyph->MUX_SYSINFO.VERSION = get_ini_unicode(muos_glyph, "muxsysinfo", "VERSION");
    glyph->MUX_SYSINFO.RETROARCH = get_ini_unicode(muos_glyph, "muxsysinfo", "RETROARCH");
    glyph->MUX_SYSINFO.KERNEL = get_ini_unicode(muos_glyph, "muxsysinfo", "KERNEL");
    glyph->MUX_SYSINFO.UPTIME = get_ini_unicode(muos_glyph, "muxsysinfo", "UPTIME");
    glyph->MUX_SYSINFO.CPU_INFO = get_ini_unicode(muos_glyph, "muxsysinfo", "CPU_INFO");
    glyph->MUX_SYSINFO.CPU_SPEED = get_ini_unicode(muos_glyph, "muxsysinfo", "CPU_SPEED");
    glyph->MUX_SYSINFO.CPU_GOVERNOR = get_ini_unicode(muos_glyph, "muxsysinfo", "CPU_GOVERNOR");
    glyph->MUX_SYSINFO.RAM = get_ini_unicode(muos_glyph, "muxsysinfo", "RAM");
    glyph->MUX_SYSINFO.TEMP = get_ini_unicode(muos_glyph, "muxsysinfo", "TEMP");
    glyph->MUX_SYSINFO.RUNNING = get_ini_unicode(muos_glyph, "muxsysinfo", "RUNNING");
    glyph->MUX_SYSINFO.BAT_CAP = get_ini_unicode(muos_glyph, "muxsysinfo", "BAT_CAP");
    glyph->MUX_SYSINFO.BAT_VOLT = get_ini_unicode(muos_glyph, "muxsysinfo", "BAT_VOLT");

    glyph->MUX_THEME.ITEM = get_ini_unicode(muos_glyph, "muxtheme", "ITEM");

    glyph->MUX_TIMEZONE.ITEM = get_ini_unicode(muos_glyph, "muxtimezone", "ITEM");

    glyph->MUX_TRACKER.ITEM = get_ini_unicode(muos_glyph, "muxtracker", "ITEM");

    glyph->MUX_TWEAK_ADVANCED.AB_SWAP = get_ini_unicode(muos_glyph, "muxtweakadv", "AB_SWAP");
    glyph->MUX_TWEAK_ADVANCED.THERMAL_ZONE = get_ini_unicode(muos_glyph, "muxtweakadv", "THERMAL_ZONE");
    glyph->MUX_TWEAK_ADVANCED.INTERFACE_FONT = get_ini_unicode(muos_glyph, "muxtweakadv", "INTERFACE_FONT");
    glyph->MUX_TWEAK_ADVANCED.VERBOSE = get_ini_unicode(muos_glyph, "muxtweakadv", "VERBOSE");
    glyph->MUX_TWEAK_ADVANCED.LOW_VOLUME = get_ini_unicode(muos_glyph, "muxtweakadv", "LOW_VOLUME");
    glyph->MUX_TWEAK_ADVANCED.BAT_OFFSET = get_ini_unicode(muos_glyph, "muxtweakadv", "OFFSET");

    glyph->MUX_TWEAK_GENERAL.HIDDEN_CONTENT = get_ini_unicode(muos_glyph, "muxtweakgen", "HIDDEN_CONTENT");
    glyph->MUX_TWEAK_GENERAL.LAUNCHER_SOUND = get_ini_unicode(muos_glyph, "muxtweakgen", "LAUNCHER_SOUND");
    glyph->MUX_TWEAK_GENERAL.STARTUP = get_ini_unicode(muos_glyph, "muxtweakgen", "STARTUP");
    glyph->MUX_TWEAK_GENERAL.POWER = get_ini_unicode(muos_glyph, "muxtweakgen", "POWER");
    glyph->MUX_TWEAK_GENERAL.LOW_BATTERY = get_ini_unicode(muos_glyph, "muxtweakgen", "LOW_BATTERY");
    glyph->MUX_TWEAK_GENERAL.COLOUR_TEMP = get_ini_unicode(muos_glyph, "muxtweakgen", "COLOUR_TEMP");
    glyph->MUX_TWEAK_GENERAL.BRIGHTNESS = get_ini_unicode(muos_glyph, "muxtweakgen", "BRIGHTNESS");
    glyph->MUX_TWEAK_GENERAL.HDMI = get_ini_unicode(muos_glyph, "muxtweakgen", "HDMI");
    glyph->MUX_TWEAK_GENERAL.INTERFACE = get_ini_unicode(muos_glyph, "muxtweakgen", "INTERFACE");
    glyph->MUX_TWEAK_GENERAL.ADVANCED = get_ini_unicode(muos_glyph, "muxtweakgen", "ADVANCED");

    glyph->MUX_VISUAL.BATTERY = get_ini_unicode(muos_glyph, "muxvisual", "BATTERY");
    glyph->MUX_VISUAL.NETWORK = get_ini_unicode(muos_glyph, "muxvisual", "NETWORK");
    glyph->MUX_VISUAL.BLUETOOTH = get_ini_unicode(muos_glyph, "muxvisual", "BLUETOOTH");
    glyph->MUX_VISUAL.CLOCK = get_ini_unicode(muos_glyph, "muxvisual", "CLOCK");
    glyph->MUX_VISUAL.CONTENT = get_ini_unicode(muos_glyph, "muxvisual", "CONTENT");

    glyph->MUX_WEBSERVICE.SHELL = get_ini_unicode(muos_glyph, "muxwebserv", "SHELL");
    glyph->MUX_WEBSERVICE.SFTP = get_ini_unicode(muos_glyph, "muxwebserv", "SFTP");
    glyph->MUX_WEBSERVICE.VIRTUAL_TERM = get_ini_unicode(muos_glyph, "muxwebserv", "VIRTUAL_TERM");
    glyph->MUX_WEBSERVICE.SYNCTHING = get_ini_unicode(muos_glyph, "muxwebserv", "SYNCTHING");
    glyph->MUX_WEBSERVICE.NTP = get_ini_unicode(muos_glyph, "muxwebserv", "NTP");

    mini_free(muos_glyph);
}
