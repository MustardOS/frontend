#include "init.h"
#include "options.h"
#include "language.h"
#include "config.h"
#include "json/json.h"
#include "fileio.h"
#include "util.h"

struct json translation_generic;
struct json translation_specific;
static char *language_json = NULL;

char *disabled_enabled[2];
char *excluded_included[2];
char *allowed_restricted[2];
char *hidden_visible[2];
char *toggle_icon_visible[3];
char *battery_display[3];
char *debug_log_mode[3];
char *scroll_speed[4];

void load_language_file(const char *module) {
    char language_file[MAX_BUFFER_SIZE];
    snprintf(language_file, sizeof(language_file), STORAGE_LANG "/%s.json", config.settings.general.language);

    char *content = read_all_char_from(language_file);
    if (!json_valid(content)) {
        free(content);
        return;
    }

    free(language_json);
    language_json = content;

    const struct json root = json_parse(language_json);
    translation_specific = json_object_get(root, module);
    translation_generic = json_object_get(root, "generic");
}

char *translate_generic(char *key) {
    const struct json translation_generic_json = json_object_get(translation_generic, key);

    if (json_exists(translation_generic_json)) {
        char translation[MAX_BUFFER_SIZE];
        json_string_copy(translation_generic_json, translation, sizeof(translation));
        return mux_strdup(translation);
    }

    return key;
}

char *translate_help(char *key) {
    return translate_generic(key);
}

char *translate_specific(char *key) {
    const struct json translation_specific_json = json_object_get(translation_specific, key);

    if (json_exists(translation_specific_json)) {
        char translation[MAX_BUFFER_SIZE];
        json_string_copy(translation_specific_json, translation, sizeof(translation));
        return mux_strdup(translation);
    }

    return key;
}

void fill_generic(const char *key, char *field, const size_t size) {
    const struct json j = json_object_get(translation_generic, key);
    if (json_exists(j)) {
        json_string_copy(j, field, size);
    } else {
        snprintf(field, size, "%s", key);
    }
}

void fill_specific(const char *key, char *field, const size_t size) {
    const struct json j = json_object_get(translation_specific, key);
    if (json_exists(j)) {
        json_string_copy(j, field, size);
    } else {
        snprintf(field, size, "%s", key);
    }
}

void common_var_init(void) {
    disabled_enabled[0] = lang.generic.disabled;
    disabled_enabled[1] = lang.generic.enabled;

    excluded_included[0] = lang.generic.excluded;
    excluded_included[1] = lang.generic.included;

    allowed_restricted[0] = lang.generic.allowed;
    allowed_restricted[1] = lang.generic.restricted;

    hidden_visible[0] = lang.generic.hidden;
    hidden_visible[1] = lang.generic.visible;

    toggle_icon_visible[0] = lang.generic.visible;
    toggle_icon_visible[1] = lang.generic.noglyph;
    toggle_icon_visible[2] = lang.generic.hidden;

    battery_display[0] = lang.generic.icon_only;
    battery_display[1] = lang.generic.text_only;
    battery_display[2] = lang.generic.text_icon;

    debug_log_mode[0] = lang.generic.disabled;
    debug_log_mode[1] = lang.generic.standard;
    debug_log_mode[2] = lang.generic.verbose;

    scroll_speed[0] = lang.generic.disabled;
    scroll_speed[1] = lang.generic.slow;
    scroll_speed[2] = lang.generic.medium;
    scroll_speed[3] = lang.generic.fast;
}

void load_lang(struct mux_lang *lang) {
    load_language_file(mux_module);

#define SYSTEM_FIELD(field, string)   snprintf((field), MAX_BUFFER_SIZE, "%s", (string))
#define GENERIC_FIELD(field, string)  fill_generic((string), (field), MAX_BUFFER_SIZE)
#define SPECIFIC_FIELD(field, string) fill_specific((string), (field), MAX_BUFFER_SIZE)

    // system language
    SYSTEM_FIELD(lang->system.no_joy_general, "Failed to open GENERAL joystick device");
    SYSTEM_FIELD(lang->system.no_joy_power, "Failed to open POWER joystick device");
    SYSTEM_FIELD(lang->system.no_joy_volume, "Failed to open VOLUME joystick device");
    SYSTEM_FIELD(lang->system.no_joy_extra, "Failed to open EXTRA joystick device");
    SYSTEM_FIELD(lang->system.fail_allocate_mem, "Failed to allocate memory");
    SYSTEM_FIELD(lang->system.fail_dup_string, "Failed to duplicate string");
    SYSTEM_FIELD(lang->system.fail_dir_open, "Failed to open directory");
    SYSTEM_FIELD(lang->system.fail_file_open, "Failed to open file");
    SYSTEM_FIELD(lang->system.fail_file_write, "Failed to open file for writing");
    SYSTEM_FIELD(lang->system.fail_file_read, "Failed to open file for reading");
    SYSTEM_FIELD(lang->system.fail_fork, "Failed to fork");
    SYSTEM_FIELD(lang->system.fail_run_command, "Failed to run command");
    SYSTEM_FIELD(lang->system.fail_read_command, "Failed read command output");
    SYSTEM_FIELD(lang->system.fail_close_command, "Failed to close command stream");
    SYSTEM_FIELD(lang->system.fail_delete_file, "Failed to delete file");
    SYSTEM_FIELD(lang->system.fail_create_file, "Failed to create file");
    SYSTEM_FIELD(lang->system.fail_stat, "Failed to retrieve file or directory status");
    SYSTEM_FIELD(lang->system.fail_proc_part, "Failed to open /proc/partitions");
    SYSTEM_FIELD(lang->system.fail_int16_length, "Failed to use int16 - out of range");

    // generic common language
    GENERIC_FIELD(lang->generic.add, "Add");
    GENERIC_FIELD(lang->generic.add_collect, "Added to Collection");
    GENERIC_FIELD(lang->generic.allowed, "Allowed");
    GENERIC_FIELD(lang->generic.back, "Back");
    GENERIC_FIELD(lang->generic.backspace, "Backspace");
    GENERIC_FIELD(lang->generic.cannot_delete_active_theme, "Cannot Delete Active Theme");
    GENERIC_FIELD(lang->generic.change, "Change");
    GENERIC_FIELD(lang->generic.channel, "Channel");
    GENERIC_FIELD(lang->generic.check, "Check");
    GENERIC_FIELD(lang->generic.clear, "Clear");
    GENERIC_FIELD(lang->generic.close, "Close");
    GENERIC_FIELD(lang->generic.confirm, "Confirm");
    GENERIC_FIELD(lang->generic.collect, "Collect");
    GENERIC_FIELD(lang->generic.content, "Content");
    GENERIC_FIELD(lang->generic.details, "Details");
    GENERIC_FIELD(lang->generic.directory, "Directory");
    GENERIC_FIELD(lang->generic.disabled, "Disabled");
    GENERIC_FIELD(lang->generic.download, "Download");
    GENERIC_FIELD(lang->generic.edit, "Edit");
    GENERIC_FIELD(lang->generic.output, "Export");
    GENERIC_FIELD(lang->generic.enabled, "Enabled");
    GENERIC_FIELD(lang->generic.excluded, "Excluded");
    GENERIC_FIELD(lang->generic.extract, "Extract");
    GENERIC_FIELD(lang->generic.extracting_archive, "Extracting Archive");
    GENERIC_FIELD(lang->generic.filter, "Filter");
    GENERIC_FIELD(lang->generic.hidden, "Hidden");
    GENERIC_FIELD(lang->generic.hold_remove, "Hold L2 and press X to confirm removal!");
    GENERIC_FIELD(lang->generic.hold_reset, "Hold L2 and press X to confirm reset!");
    GENERIC_FIELD(lang->generic.included, "Included");
    GENERIC_FIELD(lang->generic.info, "Info");
    GENERIC_FIELD(lang->generic.install, "Install");
    GENERIC_FIELD(lang->generic.kiosk_disable, "This is disabled in kiosk mode!");
    GENERIC_FIELD(lang->generic.launch, "Launch");
    GENERIC_FIELD(lang->generic.load, "Load");
    GENERIC_FIELD(lang->generic.loading, "Loading…");
    GENERIC_FIELD(lang->generic.migrate, "Migrate to SD2");
    GENERIC_FIELD(lang->generic.need_connect, "Network connection required…");
    GENERIC_FIELD(lang->generic.invalid_time, "Invalid date and time detected…");
    GENERIC_FIELD(lang->generic.new, "New");
    GENERIC_FIELD(lang->generic.noglyph, "No Glyph");
    GENERIC_FIELD(lang->generic.not_connected, "Not Connected");
    GENERIC_FIELD(lang->generic.no_help, "No Help Information Found");
    GENERIC_FIELD(lang->generic.no_info, "No Information Found");
    GENERIC_FIELD(lang->generic.no_load, "Cannot find original launch content…");
    GENERIC_FIELD(lang->generic.offline, "Offline");
    GENERIC_FIELD(lang->generic.online, "Online");
    GENERIC_FIELD(lang->generic.open, "Open");
    GENERIC_FIELD(lang->generic.option, "Options");
    GENERIC_FIELD(lang->generic.preview, "Preview");
    GENERIC_FIELD(lang->generic.previous, "Previous");
    GENERIC_FIELD(lang->generic.read, "Read");
    GENERIC_FIELD(lang->generic.rebooting, "Rebooting…");
    GENERIC_FIELD(lang->generic.recursive, "Recursive");
    GENERIC_FIELD(lang->generic.refresh, "Refresh");
    GENERIC_FIELD(lang->generic.refresh_run, "Refreshing…");
    GENERIC_FIELD(lang->generic.remove, "Remove");
    GENERIC_FIELD(lang->generic.remove_fail, "Failed to remove item…");
    GENERIC_FIELD(lang->generic.rescan, "Rescan");
    GENERIC_FIELD(lang->generic.reset, "Reset");
    GENERIC_FIELD(lang->generic.restore, "Restore");
    GENERIC_FIELD(lang->generic.restricted, "Restricted");
    GENERIC_FIELD(lang->generic.save, "Save");
    GENERIC_FIELD(lang->generic.saving, "Saving…");
    GENERIC_FIELD(lang->generic.scan, "Scan");
    GENERIC_FIELD(lang->generic.scroll, "Scroll");
    GENERIC_FIELD(lang->generic.select, "Select");
    GENERIC_FIELD(lang->generic.set, "Set");
    GENERIC_FIELD(lang->generic.shutting_down, "Shutting Down…");
    GENERIC_FIELD(lang->generic.space, "Space");
    GENERIC_FIELD(lang->generic.storage, "Storage");
    GENERIC_FIELD(lang->generic.switch_image, "Preview Image");
    GENERIC_FIELD(lang->generic.switch_info, "Information");
    GENERIC_FIELD(lang->generic.sync, "Sync to SD1");
    GENERIC_FIELD(lang->generic.toggle_all, "Toggle All");
    GENERIC_FIELD(lang->generic.top, "Top");
    GENERIC_FIELD(lang->generic.type, "Type");
    GENERIC_FIELD(lang->generic.unknown, "Unknown");
    GENERIC_FIELD(lang->generic.use, "Use");
    GENERIC_FIELD(lang->generic.user_defined, "User Defined");
    GENERIC_FIELD(lang->generic.visible, "Visible");
    GENERIC_FIELD(lang->generic.sunday, "Sunday");
    GENERIC_FIELD(lang->generic.monday, "Monday");
    GENERIC_FIELD(lang->generic.tuesday, "Tuesday");
    GENERIC_FIELD(lang->generic.wednesday, "Wednesday");
    GENERIC_FIELD(lang->generic.thursday, "Thursday");
    GENERIC_FIELD(lang->generic.friday, "Friday");
    GENERIC_FIELD(lang->generic.saturday, "Saturday");
    GENERIC_FIELD(lang->generic.cancel, "Cancel");
    GENERIC_FIELD(lang->generic.understand, "I Understand");
    GENERIC_FIELD(lang->generic.warning, "Warning");
    GENERIC_FIELD(lang->generic.skip_confirm, "Skip Dialogue");
    GENERIC_FIELD(lang->generic.unsafe_archive, "Archive contains unsafe file paths and was not extracted");
    GENERIC_FIELD(lang->generic.clean, "Clean");
    GENERIC_FIELD(lang->generic.discard, "Discard");
    GENERIC_FIELD(lang->generic.modified, "Modified");
    GENERIC_FIELD(lang->generic.icon_only, "Icon Only");
    GENERIC_FIELD(lang->generic.text_only, "Text Only");
    GENERIC_FIELD(lang->generic.text_icon, "Text + Icon");
    GENERIC_FIELD(lang->generic.standard, "Standard");
    GENERIC_FIELD(lang->generic.verbose, "Verbose");
    GENERIC_FIELD(lang->generic.slow, "Slow");
    GENERIC_FIELD(lang->generic.medium, "Medium");
    GENERIC_FIELD(lang->generic.fast, "Fast");
    GENERIC_FIELD(lang->generic.low, "Low");
    GENERIC_FIELD(lang->generic.high, "High");
    GENERIC_FIELD(lang->generic.minimal, "Minimal");
    GENERIC_FIELD(lang->generic.maximum, "Maximum");
    GENERIC_FIELD(lang->generic.horizontal, "Horizontal");
    GENERIC_FIELD(lang->generic.vertical, "Vertical");
    GENERIC_FIELD(lang->generic.up, "Up");
    GENERIC_FIELD(lang->generic.down, "Down");
    GENERIC_FIELD(lang->generic.left, "Left");
    GENERIC_FIELD(lang->generic.right, "Right");
    GENERIC_FIELD(lang->generic.all, "All");
    GENERIC_FIELD(lang->generic.ludicrous, "Ludicrous");
    GENERIC_FIELD(lang->generic.unsaved, "Unsaved Changes");
    GENERIC_FIELD(lang->generic.crash_title, "Guru Meditation Error");
    GENERIC_FIELD(lang->generic.crash_message, "If this continues, please report it to the MustardOS team!");
    GENERIC_FIELD(lang->generic.crash_module, "Module");
    GENERIC_FIELD(lang->generic.crash_fault, "Fault");
    GENERIC_FIELD(lang->generic.random, "Random");
    GENERIC_FIELD(lang->generic.power_loss_title, "Unexpected Power Loss");
    GENERIC_FIELD(
        lang->generic.power_loss_message,
        "An unexpected power loss was detected during the previous session. Please ensure your device is "
        "fully charged before use and do not use the reset button!"
    );

    // muxactivity
    SPECIFIC_FIELD(lang->muxactivity.title, "ACTIVITY TRACKER");
    SPECIFIC_FIELD(
        lang->muxactivity.help, "Tracks what you play, how often, and for how long. View detailed stats per "
                                "game, overall play habits, and export your activity data to a HTML file"
    );
    SPECIFIC_FIELD(lang->muxactivity.none, "Nothing Played Yet…");
    SPECIFIC_FIELD(lang->muxactivity.info, "Info");
    SPECIFIC_FIELD(lang->muxactivity.launch, "Launch Count");
    SPECIFIC_FIELD(lang->muxactivity.time, "Duration");
    SPECIFIC_FIELD(lang->muxactivity.html, "Export");
    SPECIFIC_FIELD(lang->muxactivity.unique, "Unique");
    SPECIFIC_FIELD(lang->muxactivity.export_success, "Activity statistics exported");
    SPECIFIC_FIELD(lang->muxactivity.export_error, "Error exporting statistics");
    SPECIFIC_FIELD(lang->muxactivity.removed, "Content playtime removed");
    SPECIFIC_FIELD(lang->muxactivity.detail.name, "Content Name");
    SPECIFIC_FIELD(lang->muxactivity.detail.core, "Core Used");
    SPECIFIC_FIELD(lang->muxactivity.detail.launch, "Launch Count");
    SPECIFIC_FIELD(lang->muxactivity.detail.device, "Last Device");
    SPECIFIC_FIELD(lang->muxactivity.detail.mode, "Last Mode");
    SPECIFIC_FIELD(lang->muxactivity.detail.played, "Last Played");
    SPECIFIC_FIELD(lang->muxactivity.detail.average, "Average Time");
    SPECIFIC_FIELD(lang->muxactivity.detail.total, "Total Time");
    SPECIFIC_FIELD(lang->muxactivity.detail.last, "Last Session");
    SPECIFIC_FIELD(lang->muxactivity.global.nav, "Overview");
    SPECIFIC_FIELD(lang->muxactivity.global.top_time, "Top Content by Time");
    SPECIFIC_FIELD(lang->muxactivity.global.top_launch, "Top Content by Launch");
    SPECIFIC_FIELD(lang->muxactivity.global.core, "Most Frequent Core");
    SPECIFIC_FIELD(lang->muxactivity.global.device, "Most Used Device");
    SPECIFIC_FIELD(lang->muxactivity.global.mode, "Most Used Mode");
    SPECIFIC_FIELD(lang->muxactivity.global.launch, "Total Launch Count");
    SPECIFIC_FIELD(lang->muxactivity.global.total, "Total Play Time");
    SPECIFIC_FIELD(lang->muxactivity.global.average, "Average Play Time");
    SPECIFIC_FIELD(lang->muxactivity.global.oldest, "Oldest Session");
    SPECIFIC_FIELD(lang->muxactivity.global.longest, "Longest Session");
    SPECIFIC_FIELD(lang->muxactivity.global.overall, "Overall Play Style");
    SPECIFIC_FIELD(lang->muxactivity.global.unique_play, "Unique Content Played");
    SPECIFIC_FIELD(lang->muxactivity.global.unique_core, "Unique Cores Used");
    SPECIFIC_FIELD(lang->muxactivity.global.active_time, "Most Active Time");
    SPECIFIC_FIELD(lang->muxactivity.global.favourite_day, "Favourite Day");
    SPECIFIC_FIELD(lang->muxactivity.style.local.label, "Play Style");
    SPECIFIC_FIELD(lang->muxactivity.style.local.one, "One and Done");
    SPECIFIC_FIELD(lang->muxactivity.style.local.sampler, "Sampler");
    SPECIFIC_FIELD(lang->muxactivity.style.local.burst, "Short Bursts");
    SPECIFIC_FIELD(lang->muxactivity.style.local.lng, "Long Sessions");
    SPECIFIC_FIELD(lang->muxactivity.style.local.completionist, "Completionist");
    SPECIFIC_FIELD(lang->muxactivity.style.local.abandoned, "Abandoned");
    SPECIFIC_FIELD(lang->muxactivity.style.local.marathoner, "Marathoner");
    SPECIFIC_FIELD(lang->muxactivity.style.local.returner, "Returner");
    SPECIFIC_FIELD(lang->muxactivity.style.local.on_off, "On and Off");
    SPECIFIC_FIELD(lang->muxactivity.style.local.weekend, "Weekend Warrior");
    SPECIFIC_FIELD(lang->muxactivity.style.local.regular, "Regular Play");
    SPECIFIC_FIELD(lang->muxactivity.style.local.comfort, "Comfort Game");
    SPECIFIC_FIELD(lang->muxactivity.style.global.label, "Overall Play Style");
    SPECIFIC_FIELD(lang->muxactivity.style.global.casual, "Casual");
    SPECIFIC_FIELD(lang->muxactivity.style.global.core, "Core Gamer");
    SPECIFIC_FIELD(lang->muxactivity.style.global.explorer, "Explorer");
    SPECIFIC_FIELD(lang->muxactivity.style.global.binger, "Binger");
    SPECIFIC_FIELD(lang->muxactivity.style.global.completionist, "Completionist");
    SPECIFIC_FIELD(lang->muxactivity.style.global.power, "Power Player");
    SPECIFIC_FIELD(lang->muxactivity.style.global.collector, "Content Collector");
    SPECIFIC_FIELD(lang->muxactivity.style.global.specialist, "Specialist");
    SPECIFIC_FIELD(lang->muxactivity.style.global.nomad, "Device Nomad");
    SPECIFIC_FIELD(lang->muxactivity.style.global.routine, "Routine Player");
    SPECIFIC_FIELD(lang->muxactivity.style.global.habitual, "Habitual Player");
    SPECIFIC_FIELD(lang->muxactivity.style.global.window, "Window Shopper");

    // muxapp
    SPECIFIC_FIELD(lang->muxapp.title, "APPLICATIONS");
    SPECIFIC_FIELD(lang->muxapp.load_app, "Loading Application");
    SPECIFIC_FIELD(lang->muxapp.no_app, "No Applications Found");
    SPECIFIC_FIELD(lang->muxapp.archive, "Archive Manager");
    SPECIFIC_FIELD(lang->muxapp.task, "Task Toolkit");

    // muxappcon
    SPECIFIC_FIELD(lang->muxappcon.title, "APPLICATION OPTION");
    SPECIFIC_FIELD(lang->muxappcon.name, "Name");
    SPECIFIC_FIELD(lang->muxappcon.governor, "Governor");
    SPECIFIC_FIELD(lang->muxappcon.control, "Control Scheme");
    SPECIFIC_FIELD(lang->muxappcon.help.governor, "Set the CPU governor for the selected application");
    SPECIFIC_FIELD(lang->muxappcon.help.control, "Set the control scheme for the selected application");

    // muxarchive
    SPECIFIC_FIELD(lang->muxarchive.title, "ARCHIVE MANAGER");
    SPECIFIC_FIELD(lang->muxarchive.installed, "INSTALLED");
    SPECIFIC_FIELD(lang->muxarchive.none, "No Archives Found");
    SPECIFIC_FIELD(
        lang->muxarchive.help,
        "Archive items can be found and installed here - Save games and states, updates, themes etc"
    );

    // muxassign
    SPECIFIC_FIELD(lang->muxassign.title, "ASSIGN");
    SPECIFIC_FIELD(lang->muxassign.dir, "Assigned to Directory");
    SPECIFIC_FIELD(lang->muxassign.file, "Assigned to Content");
    SPECIFIC_FIELD(lang->muxassign.none, "No Cores Found…");
    SPECIFIC_FIELD(lang->muxassign.help, "This is where you can assign a core or external emulator to content");
    SPECIFIC_FIELD(lang->muxassign.core_down, "Core Downloader");

    // muxbackup
    SPECIFIC_FIELD(lang->muxbackup.title, "DEVICE BACKUP");
    SPECIFIC_FIELD(lang->muxbackup.apps, "Applications");
    SPECIFIC_FIELD(lang->muxbackup.bios, "System BIOS");
    SPECIFIC_FIELD(lang->muxbackup.catalogue, "Metadata Catalogue");
    SPECIFIC_FIELD(lang->muxbackup.cheats, "RetroArch Cheats");
    SPECIFIC_FIELD(lang->muxbackup.collection, "Content Collection");
    SPECIFIC_FIELD(lang->muxbackup.config, "RetroArch Configs");
    SPECIFIC_FIELD(lang->muxbackup.content, "Content Assignment");
    SPECIFIC_FIELD(lang->muxbackup.history, "History");
    SPECIFIC_FIELD(lang->muxbackup.init, "User Init Scripts");
    SPECIFIC_FIELD(lang->muxbackup.music, "Background Music");
    SPECIFIC_FIELD(lang->muxbackup.music, "Background Music");
    SPECIFIC_FIELD(lang->muxbackup.name, "Friendly Name Configs");
    SPECIFIC_FIELD(lang->muxbackup.network, "Network Profiles");
    SPECIFIC_FIELD(lang->muxbackup.overlays, "RetroArch Overlays");
    SPECIFIC_FIELD(lang->muxbackup.override, "Content Launch Overrides");
    SPECIFIC_FIELD(lang->muxbackup.package, "Custom Packages");
    SPECIFIC_FIELD(lang->muxbackup.save, "Save Games + Save States");
    SPECIFIC_FIELD(lang->muxbackup.screenshot, "Screenshots");
    SPECIFIC_FIELD(lang->muxbackup.shaders, "RetroArch Shaders");
    SPECIFIC_FIELD(lang->muxbackup.syncthing, "Syncthing Configs");
    SPECIFIC_FIELD(lang->muxbackup.theme, "Themes");
    SPECIFIC_FIELD(lang->muxbackup.track, "Activity Tracker");
    SPECIFIC_FIELD(lang->muxbackup.target, "Backup Target");
    SPECIFIC_FIELD(lang->muxbackup.merge, "Merge Backups");
    SPECIFIC_FIELD(lang->muxbackup.start, "Start Backup");
    SPECIFIC_FIELD(lang->muxbackup.help.apps, "Location of installed applications");
    SPECIFIC_FIELD(lang->muxbackup.help.bios, "Location of system BIOS files");
    SPECIFIC_FIELD(lang->muxbackup.help.catalogue, "Location of content images and text");
    SPECIFIC_FIELD(lang->muxbackup.help.cheats, "Location of the RetroArch cheats");
    SPECIFIC_FIELD(lang->muxbackup.help.collection, "Location of content collection");
    SPECIFIC_FIELD(lang->muxbackup.help.config, "Location of RetroArch configurations");
    SPECIFIC_FIELD(lang->muxbackup.help.content, "Location of assigned content information");
    SPECIFIC_FIELD(lang->muxbackup.help.history, "Location of history");
    SPECIFIC_FIELD(lang->muxbackup.help.init, "Location of User Initialisation scripts");
    SPECIFIC_FIELD(lang->muxbackup.help.music, "Location of background music");
    SPECIFIC_FIELD(lang->muxbackup.help.name, "Location of friendly name configurations");
    SPECIFIC_FIELD(lang->muxbackup.help.network, "Location of Network Profiles");
    SPECIFIC_FIELD(lang->muxbackup.help.overlays, "Location of the RetroArch overlays");
    SPECIFIC_FIELD(lang->muxbackup.help.override, "Location of the content launch overrides");
    SPECIFIC_FIELD(lang->muxbackup.help.package, "Location of custom packages");
    SPECIFIC_FIELD(lang->muxbackup.help.save, "Location of save states and files");
    SPECIFIC_FIELD(lang->muxbackup.help.screenshot, "Location of screenshots");
    SPECIFIC_FIELD(lang->muxbackup.help.shaders, "Location of RetroArch shaders");
    SPECIFIC_FIELD(lang->muxbackup.help.syncthing, "Location of Syncthing configurations");
    SPECIFIC_FIELD(lang->muxbackup.help.theme, "Location of themes");
    SPECIFIC_FIELD(lang->muxbackup.help.track, "Location of Game Activity Tracker");
    SPECIFIC_FIELD(lang->muxbackup.help.target, "Toggle the target storage device for the backup");
    SPECIFIC_FIELD(lang->muxbackup.help.merge, "Merge all backup targets to a single archive");
    SPECIFIC_FIELD(lang->muxbackup.help.start, "Start the backup process for the selected items");

    // muxbatinfo
    SPECIFIC_FIELD(lang->muxbatinfo.title, "BATTERY DETAILS");
    SPECIFIC_FIELD(lang->muxbatinfo.capacity, "Capacity");
    SPECIFIC_FIELD(lang->muxbatinfo.voltage, "Voltage");
    SPECIFIC_FIELD(lang->muxbatinfo.status, "Status");
    SPECIFIC_FIELD(lang->muxbatinfo.health, "Health");
    SPECIFIC_FIELD(lang->muxbatinfo.design_cap, "Design Capacity");
    SPECIFIC_FIELD(lang->muxbatinfo.last_charged, "Last Charged");
    SPECIFIC_FIELD(lang->muxbatinfo.time_on_battery, "Time on Battery");
    SPECIFIC_FIELD(lang->muxbatinfo.battery_used, "Battery Used");
    SPECIFIC_FIELD(lang->muxbatinfo.charger, "Charger");
    SPECIFIC_FIELD(lang->muxbatinfo.help.capacity, "The current detected battery capacity");
    SPECIFIC_FIELD(lang->muxbatinfo.help.voltage, "The current detected battery voltage");
    SPECIFIC_FIELD(lang->muxbatinfo.help.status, "The current charging status reported by the battery");
    SPECIFIC_FIELD(lang->muxbatinfo.help.health, "The health status reported by the battery");
    SPECIFIC_FIELD(lang->muxbatinfo.help.design_cap, "The original design capacity of the battery in milliamp-hours");
    SPECIFIC_FIELD(
        lang->muxbatinfo.help.last_charged, "The last time the charger was unplugged after being connected to power"
    );
    SPECIFIC_FIELD(
        lang->muxbatinfo.help.time_on_battery,
        "Total wall-clock time since the charger was last unplugged, including suspend"
    );
    SPECIFIC_FIELD(lang->muxbatinfo.help.battery_used, "Battery percentage used since the charger was last unplugged");
    SPECIFIC_FIELD(lang->muxbatinfo.help.charger, "Detection of the charger cable");

    // muxcharge
    SPECIFIC_FIELD(lang->muxcharge.boot, "Booting System - Please Wait…");
    SPECIFIC_FIELD(lang->muxcharge.capacity, "Capacity");
    SPECIFIC_FIELD(lang->muxcharge.start, "Press START button to continue booting…");
    SPECIFIC_FIELD(lang->muxcharge.voltage, "Voltage");

    // muxchrony
    SPECIFIC_FIELD(lang->muxchrony.title, "TIME SYNC DETAILS");
    SPECIFIC_FIELD(lang->muxchrony.reference, "Reference");
    SPECIFIC_FIELD(lang->muxchrony.stratum, "Stratum");
    SPECIFIC_FIELD(lang->muxchrony.ref_time, "Reference Time");
    SPECIFIC_FIELD(lang->muxchrony.system_time, "System Offset");
    SPECIFIC_FIELD(lang->muxchrony.last_offset, "Last Offset");
    SPECIFIC_FIELD(lang->muxchrony.rms_offset, "RMS Offset");
    SPECIFIC_FIELD(lang->muxchrony.frequency, "Frequency");
    SPECIFIC_FIELD(lang->muxchrony.root_delay, "Root Delay");
    SPECIFIC_FIELD(lang->muxchrony.root_disp, "Root Dispersion");
    SPECIFIC_FIELD(lang->muxchrony.update_int, "Update Interval");
    SPECIFIC_FIELD(lang->muxchrony.leap, "Leap Status");
    SPECIFIC_FIELD(lang->muxchrony.help.reference, "The upstream time source currently selected by chrony");
    SPECIFIC_FIELD(lang->muxchrony.help.stratum, "The distance from the reference clock (lower is better)");
    SPECIFIC_FIELD(lang->muxchrony.help.ref_time, "The last time a valid update was received from the reference");
    SPECIFIC_FIELD(lang->muxchrony.help.system_time, "The offset between system time and network time");
    SPECIFIC_FIELD(lang->muxchrony.help.last_offset, "The offset measured during the last update");
    SPECIFIC_FIELD(lang->muxchrony.help.rms_offset, "The long term average of the time offset");
    SPECIFIC_FIELD(lang->muxchrony.help.frequency, "Estimated clock frequency error");
    SPECIFIC_FIELD(lang->muxchrony.help.root_delay, "Total network delay to the reference clock");
    SPECIFIC_FIELD(lang->muxchrony.help.root_disp, "Estimated maximum error relative to the reference");
    SPECIFIC_FIELD(lang->muxchrony.help.update_int, "Interval between time updates");
    SPECIFIC_FIELD(lang->muxchrony.help.leap, "Current synchronisation status of the system clock");

    // muxdistemp
    SPECIFIC_FIELD(lang->muxdistemp.title, "DISPLAY TEMPERATURE");
    SPECIFIC_FIELD(lang->muxdistemp.schedule, "Schedule");
    SPECIFIC_FIELD(lang->muxdistemp.sunrise_temp, "Sunrise Temperature");
    SPECIFIC_FIELD(lang->muxdistemp.sunset_temp, "Sunset Temperature");
    SPECIFIC_FIELD(lang->muxdistemp.sunrise_time, "Sunrise Time");
    SPECIFIC_FIELD(lang->muxdistemp.sunset_time, "Sunset Time");
    SPECIFIC_FIELD(lang->muxdistemp.temperature, "Temperature");
    SPECIFIC_FIELD(lang->muxdistemp.invalid_time, "Sunrise Time must be before Sunset Time");
    SPECIFIC_FIELD(
        lang->muxdistemp.help.schedule, "Enable or disable the sunrise and sunset colour temperature schedule"
    );
    SPECIFIC_FIELD(lang->muxdistemp.help.sunrise_temp, "Colour temperature applied during sunrise hours");
    SPECIFIC_FIELD(lang->muxdistemp.help.sunset_temp, "Colour temperature applied during sunset hours");
    SPECIFIC_FIELD(lang->muxdistemp.help.sunrise_time, "Time of day when the sunrise temperature takes effect");
    SPECIFIC_FIELD(lang->muxdistemp.help.sunset_time, "Time of day when the sunset temperature takes effect");
    SPECIFIC_FIELD(lang->muxdistemp.help.temperature, "Colour temperature applied to the display");

    // muxcolfilter
    SPECIFIC_FIELD(lang->muxcolfilter.title, "COLOUR FILTER");
    SPECIFIC_FIELD(lang->muxcolfilter.help, "Change the colour filter of your current selected content");
    SPECIFIC_FIELD(lang->muxcolfilter.none, "No Colour Filters Found…");

    // muxcollect
    SPECIFIC_FIELD(lang->muxcollect.title, "COLLECTION");
    SPECIFIC_FIELD(lang->muxcollect.none, "Nothing Saved Yet…");
    SPECIFIC_FIELD(lang->muxcollect.error.remove_file, "Error removing from Collections");
    SPECIFIC_FIELD(lang->muxcollect.error.remove_dir, "Collection folder is not empty");
    SPECIFIC_FIELD(lang->muxcollect.error.load, "Error loading content file");

    // muxconfig
    SPECIFIC_FIELD(lang->muxconfig.title, "CONFIGURATION");
    SPECIFIC_FIELD(lang->muxconfig.connect, "Connectivity");
    SPECIFIC_FIELD(lang->muxconfig.custom, "Customisation");
    SPECIFIC_FIELD(lang->muxconfig.general, "General Settings");
    SPECIFIC_FIELD(lang->muxconfig.power, "Power Settings");
    SPECIFIC_FIELD(lang->muxconfig.interface, "Interface Options");
    SPECIFIC_FIELD(lang->muxconfig.overlay, "Overlay Options");
    SPECIFIC_FIELD(lang->muxconfig.language, "Language");
    SPECIFIC_FIELD(lang->muxconfig.storage, "Storage Options");
    SPECIFIC_FIELD(lang->muxconfig.backup, "Device Backup");
    SPECIFIC_FIELD(
        lang->muxconfig.help.connect, "Connect your device via Wi-Fi, enable web services, or enable USB functions"
    );
    SPECIFIC_FIELD(lang->muxconfig.help.custom, "Customise your MustardOS setup with user created packages");
    SPECIFIC_FIELD(lang->muxconfig.help.general, "Device specific and MustardOS frontend settings can be found here");
    SPECIFIC_FIELD(
        lang->muxconfig.help.language, "Select your preferred language\n\nTranslations supported by Weblate"
    );
    SPECIFIC_FIELD(
        lang->muxconfig.help.storage, "Find out what storage device core settings and configurations are mounted"
    );
    SPECIFIC_FIELD(
        lang->muxconfig.help.backup, "Back up your content to a restorable archive for your device or others"
    );
    SPECIFIC_FIELD(lang->muxconfig.help.power, "Settings to change the power features of the device");
    SPECIFIC_FIELD(lang->muxconfig.help.interface, "Settings to change the visual aspects of the frontend");
    SPECIFIC_FIELD(lang->muxconfig.help.overlay, "Settings to change the hardware overlays of content");

    // muxconnect
    SPECIFIC_FIELD(lang->muxconnect.title, "CONNECTIVITY");
    SPECIFIC_FIELD(lang->muxconnect.bluetooth, "Bluetooth");
    SPECIFIC_FIELD(lang->muxconnect.services, "Web Services");
    SPECIFIC_FIELD(lang->muxconnect.network, "Wi-Fi Network");
    SPECIFIC_FIELD(lang->muxconnect.net_adv, "Network Settings");
    SPECIFIC_FIELD(lang->muxconnect.proxy, "Proxy Settings");
    SPECIFIC_FIELD(
        lang->muxconnect.help.services, "Toggle a range of configurable services you can access via an active network"
    );
    SPECIFIC_FIELD(lang->muxconnect.help.network, "Connect to a Wi-Fi network manually or via a saved profile");
    SPECIFIC_FIELD(lang->muxconnect.help.net_adv, "Adjust network connectivity settings");
    SPECIFIC_FIELD(
        lang->muxconnect.help.proxy, "Configure an HTTP, HTTPS, or SOCKS5 proxy for outbound network traffic"
    );
    SPECIFIC_FIELD(lang->muxconnect.help.bluetooth, "Manage Bluetooth devices and auto-connect settings");

    // muxbtall
    SPECIFIC_FIELD(lang->muxbtall.title, "BLUETOOTH");
    SPECIFIC_FIELD(lang->muxbtall.auto_connect, "Auto Connect");
    SPECIFIC_FIELD(lang->muxbtall.none, "No Paired Devices Found");
    SPECIFIC_FIELD(lang->muxbtall.loading, "Updating device list…");
    SPECIFIC_FIELD(lang->muxbtall.forget, "Forget");
    SPECIFIC_FIELD(lang->muxbtall.connect, "Connect");
    SPECIFIC_FIELD(lang->muxbtall.disconnect, "Disconnect");
    SPECIFIC_FIELD(lang->muxbtall.connected, "Connected");
    SPECIFIC_FIELD(lang->muxbtall.disconnected, "Disconnected");
    SPECIFIC_FIELD(lang->muxbtall.forget_confirm, "Forget this device?");
    SPECIFIC_FIELD(lang->muxbtall.help.auto_connect, "Automatically reconnect to paired devices on startup");

    // muxbtcon
    SPECIFIC_FIELD(lang->muxbtcon.title, "BLUETOOTH SCAN");
    SPECIFIC_FIELD(lang->muxbtcon.scan, "Scanning for Bluetooth Devices…");
    SPECIFIC_FIELD(lang->muxbtcon.none, "No Bluetooth Devices Found");
    SPECIFIC_FIELD(lang->muxbtcon.info, "Device Info");
    SPECIFIC_FIELD(lang->muxbtcon.help, "Scan for nearby Bluetooth devices and pair or connect to them");
    SPECIFIC_FIELD(lang->muxbtcon.connect, "Connecting to device…");
    SPECIFIC_FIELD(lang->muxbtcon.disconnect, "Disconnecting device…");

    // muxbtdev
    SPECIFIC_FIELD(lang->muxbtdev.title, "BLUETOOTH DEVICE");
    SPECIFIC_FIELD(lang->muxbtdev.friendly_name, "Friendly Name");
    SPECIFIC_FIELD(lang->muxbtdev.type, "Device Type");
    SPECIFIC_FIELD(lang->muxbtdev.battery, "Battery");
    SPECIFIC_FIELD(lang->muxbtdev.address, "Address");
    SPECIFIC_FIELD(lang->muxbtdev.status, "Status");
    SPECIFIC_FIELD(lang->muxbtdev.connected, "Connected");
    SPECIFIC_FIELD(lang->muxbtdev.disconnected, "Disconnected");
    SPECIFIC_FIELD(lang->muxbtdev.forget, "Forget Device");
    SPECIFIC_FIELD(lang->muxbtdev.forget_confirm, "Remove this device from the paired list?");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.audio_headset, "Headset");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.audio_headphones, "Headphones");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.audio_speaker, "Speaker");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.audio_microphone, "Microphone");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.audio_card, "Audio Card");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.input_gamepad, "Gamepad");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.input_keyboard, "Keyboard");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.input_mouse, "Mouse");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.input_combo, "Keyboard + Mouse");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.input_remote, "Remote Control");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.phone, "Phone");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.computer, "Computer");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.network, "Network");
    SPECIFIC_FIELD(lang->muxbtdev.type_name.unknown, "Unknown");
    SPECIFIC_FIELD(lang->muxbtdev.help.friendly_name, "The display name for this Bluetooth device");
    SPECIFIC_FIELD(lang->muxbtdev.help.type, "The type of Bluetooth device");
    SPECIFIC_FIELD(lang->muxbtdev.help.battery, "The battery level reported by the device");
    SPECIFIC_FIELD(lang->muxbtdev.help.address, "The Bluetooth address of this device");
    SPECIFIC_FIELD(lang->muxbtdev.help.status, "Connect or disconnect this device");
    SPECIFIC_FIELD(lang->muxbtdev.help.forget, "Remove this device from the paired list");

    // muxcontent
    SPECIFIC_FIELD(lang->muxcontent.title, "CONTENT OPTIONS");
    SPECIFIC_FIELD(lang->muxcontent.shuffle, "Shuffle Function");
    SPECIFIC_FIELD(lang->muxcontent.full_width, "Full Width Selection");
    SPECIFIC_FIELD(lang->muxcontent.launch_splash, "Launch Splash");
    SPECIFIC_FIELD(lang->muxcontent.grid_mode, "Grid Mode Content");
    SPECIFIC_FIELD(lang->muxcontent.grid_mode_art, "Grid Mode Box Art");
    SPECIFIC_FIELD(lang->muxcontent.box_art.title, "Box Art");
    SPECIFIC_FIELD(lang->muxcontent.box_art.behind, "Behind");
    SPECIFIC_FIELD(lang->muxcontent.box_art.front, "Front");
    SPECIFIC_FIELD(lang->muxcontent.box_art.fs_behind, "Fullscreen + Behind");
    SPECIFIC_FIELD(lang->muxcontent.box_art.fs_front, "Fullscreen + Front");
    SPECIFIC_FIELD(lang->muxcontent.box_art.original, "Original");
    SPECIFIC_FIELD(lang->muxcontent.box_art.scale, "Box Art Scale");
    SPECIFIC_FIELD(lang->muxcontent.box_art.padding, "Box Art Padding");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.title, "Box Art Transition");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.fade_in, "Fade In");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.slide_right, "Slide From Right");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.slide_left, "Slide From Left");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.slide_up, "Slide From Bottom");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.slide_down, "Slide From Top");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.bounce_right, "Bounce From Right");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.bounce_left, "Bounce From Left");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.bounce_up, "Bounce From Bottom");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.bounce_down, "Bounce From Top");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.shoot_right, "Shoot From Right");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.shoot_left, "Shoot From Left");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.shoot_up, "Shoot From Bottom");
    SPECIFIC_FIELD(lang->muxcontent.box_art.transition.shoot_down, "Shoot From Top");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.title, "Box Art Alignment");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.b_left, "Bottom Left");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.b_mid, "Bottom Middle");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.b_right, "Bottom Right");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.m_left, "Middle Left");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.m_mid, "Center");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.m_right, "Middle Right");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.t_left, "Top Left");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.t_mid, "Top Middle");
    SPECIFIC_FIELD(lang->muxcontent.box_art.align.t_right, "Top Right");
    SPECIFIC_FIELD(lang->muxcontent.launch_swap.title, "Save State Launch");
    SPECIFIC_FIELD(lang->muxcontent.launch_swap.press_a, "Press A");
    SPECIFIC_FIELD(lang->muxcontent.launch_swap.hold_a, "Hold A");
    SPECIFIC_FIELD(lang->muxcontent.launch_swap.load_state, "Load State");
    SPECIFIC_FIELD(lang->muxcontent.launch_swap.start_fresh, "Start Fresh");
    SPECIFIC_FIELD(lang->muxcontent.video_preview.title, "Video Preview");
    SPECIFIC_FIELD(lang->muxcontent.video_preview.delay_3, "3 Second Delay");
    SPECIFIC_FIELD(lang->muxcontent.video_preview.delay_5, "5 Second Delay");
    SPECIFIC_FIELD(lang->muxcontent.video_preview.delay_10, "10 Second Delay");
    SPECIFIC_FIELD(
        lang->muxcontent.help.launch_swap,
        "Switch between pressing A or holding A to launch content save state automatically"
    );
    SPECIFIC_FIELD(lang->muxcontent.help.shuffle, "Toggles the ability to shuffle content using the R2 button");
    SPECIFIC_FIELD(lang->muxcontent.help.box_art_image, "Change the display priority of the content images");
    SPECIFIC_FIELD(lang->muxcontent.help.box_art_align, "Change the screen alignment of the content images");
    SPECIFIC_FIELD(lang->muxcontent.help.full_width, "Toggle if content highlight uses full width of device screen");
    SPECIFIC_FIELD(lang->muxcontent.help.launch_splash, "Toggle the splash image on content launching");
    SPECIFIC_FIELD(lang->muxcontent.help.grid_mode, "Allow grid mode for content");
    SPECIFIC_FIELD(
        lang->muxcontent.help.grid_mode_art,
        "Show or hide system box art images in Content Explorer when theme uses grid mode"
    );
    SPECIFIC_FIELD(lang->muxcontent.help.box_art_scale, "Scale box art to a percentage of the available display area");
    SPECIFIC_FIELD(
        lang->muxcontent.help.box_art_padding, "Add percentage padding around the box art image on all sides"
    );
    SPECIFIC_FIELD(
        lang->muxcontent.help.box_art_transition, "Select the animation for box art when navigating content"
    );
    SPECIFIC_FIELD(
        lang->muxcontent.help.video_preview, "Play a video preview after staying on content for a set duration"
    );

    // muxcontrol
    SPECIFIC_FIELD(lang->muxcontrol.title, "CONTROL");
    SPECIFIC_FIELD(lang->muxcontrol.help, "Change the control scheme of your current selected content");
    SPECIFIC_FIELD(lang->muxcontrol.none, "No Control Schemes Found…");

    // muxcustom
    SPECIFIC_FIELD(lang->muxcustom.title, "CUSTOMISATION");
    SPECIFIC_FIELD(lang->muxcustom.catalogue, "Catalogue Sets");
    SPECIFIC_FIELD(lang->muxcustom.config, "RetroArch Configurations");
    SPECIFIC_FIELD(lang->muxcustom.gridmodecontent, "Content Grid Mode");
    SPECIFIC_FIELD(lang->muxcustom.theme, "Theme Picker");
    SPECIFIC_FIELD(lang->muxcustom.themeopt, "Theme Options");
    SPECIFIC_FIELD(lang->muxcustom.themescaling, "Theme Scaling");
    SPECIFIC_FIELD(lang->muxcustom.themeresolution, "Theme Resolution");
    SPECIFIC_FIELD(lang->muxcustom.screen, "Screen");
    SPECIFIC_FIELD(lang->muxcustom.themealternate, "Alternative Theme");
    SPECIFIC_FIELD(lang->muxcustom.launchsplash, "Content Launch Splash");
    SPECIFIC_FIELD(lang->muxcustom.blackfade, "Black Fade Animation");
    SPECIFIC_FIELD(lang->muxcustom.videowallpaper, "Video Wallpaper");
    SPECIFIC_FIELD(lang->muxcustom.backgroundscale, "Background Scale");
    SPECIFIC_FIELD(lang->muxcustom.shuffle, "Content Shuffle");
    SPECIFIC_FIELD(lang->muxcustom.contentwidth, "Content Full Width");
    SPECIFIC_FIELD(lang->muxcustom.box_art.title, "Content Box Art");
    SPECIFIC_FIELD(lang->muxcustom.box_art.behind, "Behind");
    SPECIFIC_FIELD(lang->muxcustom.box_art.front, "Front");
    SPECIFIC_FIELD(lang->muxcustom.box_art.fs_behind, "Fullscreen + Behind");
    SPECIFIC_FIELD(lang->muxcustom.box_art.fs_front, "Fullscreen + Front");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.title, "Content Box Art Alignment");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.b_left, "Bottom Left");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.b_mid, "Bottom Middle");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.b_right, "Bottom Right");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.m_left, "Middle Left");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.m_mid, "Center");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.m_right, "Middle Right");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.t_left, "Top Left");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.t_mid, "Top Middle");
    SPECIFIC_FIELD(lang->muxcustom.box_art.align.t_right, "Top Right");
    SPECIFIC_FIELD(lang->muxcustom.box_art.hide_grid_mode, "Hide Grid Mode Box Art");
    SPECIFIC_FIELD(lang->muxcustom.font.title, "Font Picker");
    SPECIFIC_FIELD(lang->muxcustom.music.title, "Background Music");
    SPECIFIC_FIELD(lang->muxcustom.music.volume, "Background Music Volume");
    SPECIFIC_FIELD(lang->muxcustom.music.set, "Setting Background Volume");
    SPECIFIC_FIELD(lang->muxcustom.music.global, "Global");
    SPECIFIC_FIELD(lang->muxcustom.music.theme, "Theme");
    SPECIFIC_FIELD(lang->muxcustom.sound.title, "Navigation Sound");
    SPECIFIC_FIELD(lang->muxcustom.sound.volume, "Navigation Sound Volume");
    SPECIFIC_FIELD(lang->muxcustom.sound.set, "Setting Navigation Sound Volume");
    SPECIFIC_FIELD(lang->muxcustom.sound.global, "Global");
    SPECIFIC_FIELD(lang->muxcustom.sound.theme, "Theme");
    SPECIFIC_FIELD(lang->muxcustom.launch_swap.title, "Content Save State Launch");
    SPECIFIC_FIELD(lang->muxcustom.launch_swap.press_a, "Press A");
    SPECIFIC_FIELD(lang->muxcustom.launch_swap.hold_a, "Hold A");
    SPECIFIC_FIELD(lang->muxcustom.launch_swap.load_state, "Load State");
    SPECIFIC_FIELD(lang->muxcustom.launch_swap.start_fresh, "Start Fresh");
    SPECIFIC_FIELD(lang->muxcustom.scaling.no_scale, "None");
    SPECIFIC_FIELD(lang->muxcustom.scaling.scale, "Scale");
    SPECIFIC_FIELD(lang->muxcustom.scaling.stretch, "Stretch");
    SPECIFIC_FIELD(lang->muxcustom.chime, "Startup Chime");
    SPECIFIC_FIELD(lang->muxcustom.help.catalogue, "Load user created artwork catalogue for content");
    SPECIFIC_FIELD(lang->muxcustom.help.config, "Load user created RetroArch configurations");
    SPECIFIC_FIELD(lang->muxcustom.help.gridmodecontent, "Allow grid mode for content");
    SPECIFIC_FIELD(lang->muxcustom.help.theme, "Change the appearance of the MustardOS frontend launcher");
    SPECIFIC_FIELD(lang->muxcustom.help.theme_opt, "Override theme header, footer, and content item dimensions");
    SPECIFIC_FIELD(lang->muxcustom.help.theme_resolution, "Allows for testing different theme resolutions");
    SPECIFIC_FIELD(
        lang->muxcustom.help.theme_scaling, "Controls the type of scaling used when applying Theme Resolution setting"
    );
    SPECIFIC_FIELD(lang->muxcustom.help.theme_alternate, "Switch between different theme alternatives");
    SPECIFIC_FIELD(lang->muxcustom.help.launchsplash, "Toggle the splash image on content launching");
    SPECIFIC_FIELD(lang->muxcustom.help.black_fade, "Toggle the fade to black animation on content launching");
    SPECIFIC_FIELD(lang->muxcustom.help.video_wallpaper, "Enable background video wallpaper from the active theme");
    SPECIFIC_FIELD(
        lang->muxcustom.help.background_scale,
        "Controls how the background image or video wallpaper is scaled to fit the screen"
    );
    SPECIFIC_FIELD(lang->muxcustom.help.boxartimage, "Change the display priority of the content images");
    SPECIFIC_FIELD(lang->muxcustom.help.boxartalign, "Change the screen alignment of the content images");
    SPECIFIC_FIELD(lang->muxcustom.help.contentwidth, "Toggle if content highlight uses full width of device screen");
    SPECIFIC_FIELD(
        lang->muxcustom.help.boxarthide, "Hide system box art images in Content Explorer when theme uses grid mode"
    );
    SPECIFIC_FIELD(lang->muxcustom.help.content_options, "Configure content display, box art, and launch behaviour");
    SPECIFIC_FIELD(lang->muxcustom.content, "Content Options");
    SPECIFIC_FIELD(
        lang->muxcustom.help.font,
        "Open font settings to configure font type, custom TTF font, and size overrides for most sections"
    );
    SPECIFIC_FIELD(lang->muxcustom.help.music, "Toggle the background music of the frontend");
    SPECIFIC_FIELD(
        lang->muxcustom.help.music_volume,
        "The volume of the background music currently playing, press A to set current value"
    );
    SPECIFIC_FIELD(lang->muxcustom.help.sound, "Toggle the navigation sound of the frontend");
    SPECIFIC_FIELD(
        lang->muxcustom.help.sound_volume, "The volume of the navigation sounds, press A to set current value"
    );
    SPECIFIC_FIELD(lang->muxcustom.help.chime, "Toggle the startup chime of the frontend");
    SPECIFIC_FIELD(lang->muxcustom.help.shuffle, "Toggles the ability to shuffle content using the R2 button");
    SPECIFIC_FIELD(
        lang->muxcustom.help.launchswap,
        "Switch between pressing A or holding A to launch content save state automatically"
    );

    // muxfont
    SPECIFIC_FIELD(lang->muxfont.title, "FONT PICKER");
    SPECIFIC_FIELD(lang->muxfont.type, "Font Type");
    SPECIFIC_FIELD(lang->muxfont.name, "Font Name");
    SPECIFIC_FIELD(lang->muxfont.list_size, "List Font Size");
    SPECIFIC_FIELD(lang->muxfont.header_size, "Header Font Size");
    SPECIFIC_FIELD(lang->muxfont.footer_size, "Footer Font Size");
    SPECIFIC_FIELD(lang->muxfont.panel_size, "Panel Font Size");
    SPECIFIC_FIELD(lang->muxfont.size_default, "Default");
    SPECIFIC_FIELD(lang->muxfont.none, "No Fonts Available");
    SPECIFIC_FIELD(lang->muxfont.type_options.language, "Language");
    SPECIFIC_FIELD(lang->muxfont.type_options.theme, "Theme");
    SPECIFIC_FIELD(lang->muxfont.type_options.internal, "Internal");
    SPECIFIC_FIELD(
        lang->muxfont.help.type,
        "Select the font type: Language uses the built-in language font, Theme uses fonts provided by the "
        "active theme, Custom lets you pick a specific TTF font"
    );
    SPECIFIC_FIELD(
        lang->muxfont.help.name, "Select the custom TTF font to use (only applies when Font Type is Custom)"
    );
    SPECIFIC_FIELD(lang->muxfont.help.list_size, "Set the font size for list items (0 uses the device default)");
    SPECIFIC_FIELD(lang->muxfont.help.header_size, "Set the font size for the header bar (0 uses the device default)");
    SPECIFIC_FIELD(lang->muxfont.help.footer_size, "Set the font size for the footer bar (0 uses the device default)");
    SPECIFIC_FIELD(
        lang->muxfont.help.panel_size,
        "Set the font size for grid panel labels (0 uses the device default, only applies in grid mode)"
    );

    // muxdanger
    SPECIFIC_FIELD(lang->muxdanger.title, "DANGER SETTINGS");
    SPECIFIC_FIELD(lang->muxdanger.vmswap, "Swap Tendency");
    SPECIFIC_FIELD(lang->muxdanger.dirtyratio, "Write Back Threshold");
    SPECIFIC_FIELD(lang->muxdanger.dirtyback, "Background Write Back");
    SPECIFIC_FIELD(lang->muxdanger.cachepressure, "Cache Reclamation");
    SPECIFIC_FIELD(lang->muxdanger.nomerge, "I/O Merge Policy");
    SPECIFIC_FIELD(lang->muxdanger.nrrequests, "Queue Depth");
    SPECIFIC_FIELD(lang->muxdanger.readahead, "Read Ahead Size");
    SPECIFIC_FIELD(lang->muxdanger.pagecluster, "Swap Read-Ahead");
    SPECIFIC_FIELD(lang->muxdanger.timeslice, "Realtime Timeslice");
    SPECIFIC_FIELD(lang->muxdanger.iostats, "I/O Stats");
    SPECIFIC_FIELD(lang->muxdanger.idleflush, "Idle Flush Mode");
    SPECIFIC_FIELD(lang->muxdanger.childfirst, "Fork Optimisation");
    SPECIFIC_FIELD(lang->muxdanger.tunescale, "Scheduler Scaling");
    SPECIFIC_FIELD(lang->muxdanger.cardmode, "Disk Tuning");
    SPECIFIC_FIELD(lang->muxdanger.state, "Suspend Power State");
    SPECIFIC_FIELD(
        lang->muxdanger.help.vm_swap,
        "Controls how aggressively the system swaps memory to disk\n\nLower values keep processes in RAM longer"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.dirty_ratio,
        "Percentage of RAM allowed to hold unwritten (dirty) data before forcing a write to disk"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.dirty_back, "Background write back starts when dirty data exceeds this "
                                         "percentage of RAM\n\nLower values help reduce latency"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.cache_pressure,
        "Higher values reclaim cached file data more aggressively\n\nLower values improve performance for "
        "repeated file access"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.no_merge,
        "Controls how kernel merges I/O requests\n\nDisabling merges may benefit specific flash storage performance"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.nr_requests, "Sets the maximum number of queued I/O requests per "
                                          "device\n\nHigher values increase throughput but use more RAM"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.read_ahead, "Amount of data preloaded in memory before it's requested\n\nLarger "
                                         "values improve sequential read performance"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.page_cluster,
        "Number of pages read ahead when swapping\n\nLower values = less latency, higher = more efficient bulk reads"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.time_slice, "How long real-time tasks run before switching\n\nLower values "
                                         "favour responsiveness, higher values favour throughput"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.io_stats,
        "Enables or disables tracking of per-device I/O statistics\n\nDisabling may improve performance slightly"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.idle_flush, "Flushes dirty data on idle instead of using write back "
                                         "thresholds\n\nUseful for flash storage or reducing idle power"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.child_first,
        "When enabled, a newly forked child process runs before the parent\n\nMay improve load times for some apps"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.tune_scale,
        "Automatically adjusts scheduler behaviour based on CPU count\n\nDisable for consistent tuning on devices"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.card_mode,
        "Switch between different storage tuning options\n\nMay improve performance on certain mSD cards"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.help.state,
        "Switch between system sleep suspend states\n\nChanges how the device reacts to sleep mode and wake locks"
    );
    SPECIFIC_FIELD(
        lang->muxdanger.warn,
        "These are low level kernel parameters.\n\nIncorrect values can cause system instability or data loss!"
    );

    // muxdevice
    SPECIFIC_FIELD(lang->muxdevice.title, "DEVICE SETTINGS");
    SPECIFIC_FIELD(lang->muxdevice.hasbluetooth, "Bluetooth Integration");
    SPECIFIC_FIELD(lang->muxdevice.hasrgb, "RGB LED Integration");
    SPECIFIC_FIELD(lang->muxdevice.hasdebugfs, "Kernel Level DebugFS");
    SPECIFIC_FIELD(lang->muxdevice.hashdmi, "HDMI Integration");
    SPECIFIC_FIELD(lang->muxdevice.haslid, "Lid Switch Integration");
    SPECIFIC_FIELD(lang->muxdevice.hasnetwork, "Network Integration");
    SPECIFIC_FIELD(lang->muxdevice.hasportmaster, "Portmaster Support");
    SPECIFIC_FIELD(lang->muxdevice.help.has_bluetooth, "Toggles Bluetooth integration on device");
    SPECIFIC_FIELD(lang->muxdevice.help.has_rgb, "Toggles RGB LED integration on device");
    SPECIFIC_FIELD(
        lang->muxdevice.help.has_debugfs,
        "Toggles kernel level DebugFS mount on startup\n\nBest to leave this enabled unless otherwise advised!"
    );
    SPECIFIC_FIELD(lang->muxdevice.help.has_hdmi, "Toggles HDMI integration on device");
    SPECIFIC_FIELD(
        lang->muxdevice.help.has_lid,
        "Toggles hall switch (lid) support on device\n\nDo not enable this on non-lid devices!"
    );
    SPECIFIC_FIELD(lang->muxdevice.help.has_network, "Toggles Network integration on device");
    SPECIFIC_FIELD(lang->muxdevice.help.has_portmaster, "Toggles internal Portmaster support on device");

    // muxdownload
    SPECIFIC_FIELD(lang->muxdownload.title.core, "CORE DOWNLOADER");
    SPECIFIC_FIELD(lang->muxdownload.title.app, "APP DOWNLOADER");
    SPECIFIC_FIELD(lang->muxdownload.down.archive, "Downloading Archive");
    SPECIFIC_FIELD(lang->muxdownload.down.data, "Downloading Data");
    SPECIFIC_FIELD(lang->muxdownload.archive_removed, "Archive Removed");
    SPECIFIC_FIELD(lang->muxdownload.error_get_data, "Error Retrieving Data");

    // muxgov
    SPECIFIC_FIELD(lang->muxgov.title, "GOVERNOR");
    SPECIFIC_FIELD(
        lang->muxgov.help, "Configure CPU governors to dynamically adjust the CPU frequency and help "
                           "balance power consumption and performance"
    );
    SPECIFIC_FIELD(lang->muxgov.none, "No Governors Found…");

    // muxhdmi
    SPECIFIC_FIELD(lang->muxhdmi.title, "HDMI SETTINGS");
    SPECIFIC_FIELD(lang->muxhdmi.resolution, "Resolution");
    SPECIFIC_FIELD(lang->muxhdmi.colour.depth, "Colour Depth");
    SPECIFIC_FIELD(lang->muxhdmi.colour.space, "Colour Space");
    SPECIFIC_FIELD(lang->muxhdmi.colour.range.title, "Colour Range");
    SPECIFIC_FIELD(lang->muxhdmi.colour.range.full, "Full");
    SPECIFIC_FIELD(lang->muxhdmi.colour.range.limited, "Limited");
    SPECIFIC_FIELD(lang->muxhdmi.scan_scale.title, "Scan Scaling");
    SPECIFIC_FIELD(lang->muxhdmi.scan_scale.over, "Over");
    SPECIFIC_FIELD(lang->muxhdmi.scan_scale.under, "Under");
    SPECIFIC_FIELD(lang->muxhdmi.help.resolution, "Select the resolution for HDMI output, such as 720p or 1080p");
    SPECIFIC_FIELD(lang->muxhdmi.help.scan, "Switch between overscan or underscan to fit the display screen");
    SPECIFIC_FIELD(lang->muxhdmi.help.depth, "Set the colour depth, such as 8-bit or 10-bit");
    SPECIFIC_FIELD(lang->muxhdmi.help.range, "Set the colour range of RGB colour space");
    SPECIFIC_FIELD(lang->muxhdmi.help.space, "Set the colour space, such as RGB or YUV");

    // muxhistory
    SPECIFIC_FIELD(lang->muxhistory.title, "HISTORY");
    SPECIFIC_FIELD(lang->muxhistory.none, "Nothing Played Yet…");

    // muxinfo
    SPECIFIC_FIELD(lang->muxinfo.title, "INFORMATION");
    SPECIFIC_FIELD(lang->muxinfo.news, "Community News");
    SPECIFIC_FIELD(lang->muxinfo.sysinfo, "System Details");
    SPECIFIC_FIELD(lang->muxinfo.batinfo, "Battery Details");
    SPECIFIC_FIELD(lang->muxinfo.netinfo, "Network Details");
    SPECIFIC_FIELD(lang->muxinfo.activity, "Activity Tracker");
    SPECIFIC_FIELD(lang->muxinfo.screenshot, "Screenshots");
    SPECIFIC_FIELD(lang->muxinfo.space, "Storage Space");
    SPECIFIC_FIELD(lang->muxinfo.tester, "Input Tester");
    SPECIFIC_FIELD(lang->muxinfo.chrony, "Time Sync Details");
    SPECIFIC_FIELD(lang->muxinfo.credit, "Supporters and Credits");
    SPECIFIC_FIELD(lang->muxinfo.help.news, "Read various community news and other live information");
    SPECIFIC_FIELD(lang->muxinfo.help.sys_info, "Access version information and system details");
    SPECIFIC_FIELD(lang->muxinfo.help.bat_info, "Access detailed battery information and battery usage statistics");
    SPECIFIC_FIELD(lang->muxinfo.help.net_info, "Access network information");
    SPECIFIC_FIELD(lang->muxinfo.help.activity, "View all tracked play time data");
    SPECIFIC_FIELD(lang->muxinfo.help.screenshot, "View all of the screenshots taken on the device");
    SPECIFIC_FIELD(lang->muxinfo.help.space, "View the current used space of the mounted storage devices");
    SPECIFIC_FIELD(lang->muxinfo.help.tester, "Test the controls of the device");
    SPECIFIC_FIELD(
        lang->muxinfo.help.chrony,
        "View the current details of the device time synchronisation via Chrony service tracking"
    );
    SPECIFIC_FIELD(lang->muxinfo.help.credit, "View all of the current MustardOS supporters and extra credits");

    // muxinstall
    SPECIFIC_FIELD(lang->muxinstall.title, "INSTALLER");
    SPECIFIC_FIELD(lang->muxinstall.rtc, "Date and Time");
    SPECIFIC_FIELD(lang->muxinstall.language, "Language");
    SPECIFIC_FIELD(lang->muxinstall.install, "Install");
    SPECIFIC_FIELD(lang->muxinstall.shutdown, "Shutdown");
    SPECIFIC_FIELD(lang->muxinstall.abbr.rtc, "Clock");
    SPECIFIC_FIELD(lang->muxinstall.abbr.language, "Language");
    SPECIFIC_FIELD(lang->muxinstall.abbr.install, "Install");
    SPECIFIC_FIELD(lang->muxinstall.abbr.shutdown, "Shutdown");
    SPECIFIC_FIELD(lang->muxinstall.help.rtc, "Change your current date, time, and timezone");
    SPECIFIC_FIELD(
        lang->muxinstall.help.language, "Select your preferred language\n\nTranslations supported by Weblate"
    );
    SPECIFIC_FIELD(lang->muxinstall.help.install, "Prepare and install MustardOS");
    SPECIFIC_FIELD(lang->muxinstall.help.shutdown, "Shut down your device safely");

    // muxkiosk
    SPECIFIC_FIELD(lang->muxkiosk.title, "KIOSK SETTINGS");
    SPECIFIC_FIELD(lang->muxkiosk.enable, "Kiosk Mode");
    SPECIFIC_FIELD(lang->muxkiosk.message, "Restricted Messages");
    SPECIFIC_FIELD(lang->muxkiosk.archive, "Archive Manager");
    SPECIFIC_FIELD(lang->muxkiosk.task, "Task Toolkit");
    SPECIFIC_FIELD(lang->muxkiosk.custom, "Customisation");
    SPECIFIC_FIELD(lang->muxkiosk.language, "Language");
    SPECIFIC_FIELD(lang->muxkiosk.network, "Wi-Fi Network");
    SPECIFIC_FIELD(lang->muxkiosk.storage, "Storage");
    SPECIFIC_FIELD(lang->muxkiosk.backup, "Backup");
    SPECIFIC_FIELD(lang->muxkiosk.netadv, "Network Settings");
    SPECIFIC_FIELD(lang->muxkiosk.webserv, "Web Services");
    SPECIFIC_FIELD(lang->muxkiosk.core, "Content Core");
    SPECIFIC_FIELD(lang->muxkiosk.governor, "Content Governor");
    SPECIFIC_FIELD(lang->muxkiosk.control, "Content Control Scheme");
    SPECIFIC_FIELD(lang->muxkiosk.option, "Content Options");
    SPECIFIC_FIELD(lang->muxkiosk.retroarch, "RetroArch Settings");
    SPECIFIC_FIELD(lang->muxkiosk.search, "Content Search");
    SPECIFIC_FIELD(lang->muxkiosk.tag, "Content Tag");
    SPECIFIC_FIELD(lang->muxkiosk.colfilter, "Colour Filter");
    SPECIFIC_FIELD(lang->muxkiosk.remconfig, "Remove Config");
    SPECIFIC_FIELD(lang->muxkiosk.catalogue, "Custom Catalogue");
    SPECIFIC_FIELD(lang->muxkiosk.raconfig, "Custom RetroArch Configs");
    SPECIFIC_FIELD(lang->muxkiosk.theme, "Custom Themes");
    SPECIFIC_FIELD(lang->muxkiosk.themedown, "Theme Download");
    SPECIFIC_FIELD(lang->muxkiosk.clock, "Date and Time");
    SPECIFIC_FIELD(lang->muxkiosk.timezone, "Timezone");
    SPECIFIC_FIELD(lang->muxkiosk.apps, "Applications");
    SPECIFIC_FIELD(lang->muxkiosk.config, "Configuration");
    SPECIFIC_FIELD(lang->muxkiosk.explore, "Content Explorer");
    SPECIFIC_FIELD(lang->muxkiosk.info, "Information");
    SPECIFIC_FIELD(lang->muxkiosk.rgb, "Device Light Settings");
    SPECIFIC_FIELD(lang->muxkiosk.advanced, "Advanced Settings");
    SPECIFIC_FIELD(lang->muxkiosk.general, "General Settings");
    SPECIFIC_FIELD(lang->muxkiosk.hdmi, "HDMI Settings");
    SPECIFIC_FIELD(lang->muxkiosk.power, "Power Settings");
    SPECIFIC_FIELD(lang->muxkiosk.visual, "Interface Options");
    SPECIFIC_FIELD(lang->muxkiosk.overlay, "Overlay Options");
    SPECIFIC_FIELD(lang->muxkiosk.collection.main, "Collection Viewing");
    SPECIFIC_FIELD(lang->muxkiosk.collection.add_content, "Collection Adding Content");
    SPECIFIC_FIELD(lang->muxkiosk.collection.new_dir, "Collection Folder Creation");
    SPECIFIC_FIELD(lang->muxkiosk.collection.remove, "Collection Removal");
    SPECIFIC_FIELD(lang->muxkiosk.collection.access, "Collection Access");
    SPECIFIC_FIELD(lang->muxkiosk.history.main, "History Viewing");
    SPECIFIC_FIELD(lang->muxkiosk.history.remove, "History Removal");
    SPECIFIC_FIELD(lang->muxkiosk.help.enable, "Enable or disable kiosk mode restrictions");
    SPECIFIC_FIELD(lang->muxkiosk.help.message, "Enable or disable kiosk mode messages");
    SPECIFIC_FIELD(lang->muxkiosk.help.archive, "Allow access to archive manager");
    SPECIFIC_FIELD(lang->muxkiosk.help.task, "Permit access to the task toolkit");
    SPECIFIC_FIELD(lang->muxkiosk.help.custom, "Allow users to customise the interface");
    SPECIFIC_FIELD(lang->muxkiosk.help.language, "Permit changing the system language");
    SPECIFIC_FIELD(lang->muxkiosk.help.network, "Allow editing of network settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.storage, "Allow user to migrate or sync user based content");
    SPECIFIC_FIELD(lang->muxkiosk.help.backup, "Allow the use of the user based content backup tool");
    SPECIFIC_FIELD(lang->muxkiosk.help.net_adv, "Enable access to network settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.web_serv, "Enable use of web based features");
    SPECIFIC_FIELD(lang->muxkiosk.help.core, "Allow selection of content cores");
    SPECIFIC_FIELD(lang->muxkiosk.help.governor, "Allow selection of content governor");
    SPECIFIC_FIELD(lang->muxkiosk.help.control, "Allow selection of content control scheme");
    SPECIFIC_FIELD(lang->muxkiosk.help.option, "Allow users to adjust content options");
    SPECIFIC_FIELD(lang->muxkiosk.help.retro_arch, "Enable or restrict RetroArch settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.search, "Allow searching for content");
    SPECIFIC_FIELD(lang->muxkiosk.help.tag, "Permit tagging and metadata changes");
    SPECIFIC_FIELD(lang->muxkiosk.help.col_filter, "Permit changes to content colour filter");
    SPECIFIC_FIELD(lang->muxkiosk.help.catalogue, "Allow access to install content catalogues");
    SPECIFIC_FIELD(lang->muxkiosk.help.ra_config, "Allow access to use custom RetroArch configs");
    SPECIFIC_FIELD(lang->muxkiosk.help.theme, "Permit changing system themes");
    SPECIFIC_FIELD(lang->muxkiosk.help.theme_down, "Permit downloading of themes");
    SPECIFIC_FIELD(lang->muxkiosk.help.clock, "Allow changing the system clock");
    SPECIFIC_FIELD(lang->muxkiosk.help.timezone, "Permit adjusting the timezone");
    SPECIFIC_FIELD(lang->muxkiosk.help.apps, "Enable access to applications");
    SPECIFIC_FIELD(lang->muxkiosk.help.config, "Enable access to system configuration");
    SPECIFIC_FIELD(lang->muxkiosk.help.explore, "Permit access to the content explorer");
    SPECIFIC_FIELD(lang->muxkiosk.help.info, "Allow viewing system information");
    SPECIFIC_FIELD(lang->muxkiosk.help.rgb, "Enable access to device light settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.advanced, "Enable access to advanced settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.general, "Enable access to general settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.hdmi, "Allow modifying HDMI display settings");
    SPECIFIC_FIELD(lang->muxkiosk.help.power, "Permit power options such as shutdown or sleep");
    SPECIFIC_FIELD(lang->muxkiosk.help.visual, "Allow changes to interface visuals");
    SPECIFIC_FIELD(lang->muxkiosk.help.overlay, "Allow changes to content overlays");
    SPECIFIC_FIELD(lang->muxkiosk.help.collect_mod, "Enable viewing of content collections");
    SPECIFIC_FIELD(lang->muxkiosk.help.collect_add, "Permit adding content to collections");
    SPECIFIC_FIELD(lang->muxkiosk.help.collect_new, "Allow creation of new collection folders");
    SPECIFIC_FIELD(lang->muxkiosk.help.collect_rem, "Permit deletion of collections");
    SPECIFIC_FIELD(lang->muxkiosk.help.collect_acc, "Restrict to specific 'kiosk' directory within collections");
    SPECIFIC_FIELD(lang->muxkiosk.help.history_mod, "Enable access to recently used content");
    SPECIFIC_FIELD(lang->muxkiosk.help.history_rem, "Allow clearing items from history");

    // muxlanguage
    SPECIFIC_FIELD(lang->muxlanguage.title, "LANGUAGE");
    SPECIFIC_FIELD(lang->muxlanguage.none, "No Languages Found…");
    SPECIFIC_FIELD(lang->muxlanguage.save, "Saving Language");
    SPECIFIC_FIELD(lang->muxlanguage.help, "Select your preferred language");
    SPECIFIC_FIELD(lang->muxlanguage.downloading, "Downloading Language Updates");
    SPECIFIC_FIELD(lang->muxlanguage.error_get_data, "Error Retrieving Language Data");

    // muxlaunch
    SPECIFIC_FIELD(lang->muxlaunch.title, "MAIN MENU");
    SPECIFIC_FIELD(lang->muxlaunch.apps, "Applications");
    SPECIFIC_FIELD(lang->muxlaunch.config, "Configuration");
    SPECIFIC_FIELD(lang->muxlaunch.info, "Information");
    SPECIFIC_FIELD(lang->muxlaunch.collection, "Collection");
    SPECIFIC_FIELD(lang->muxlaunch.history, "History");
    SPECIFIC_FIELD(lang->muxlaunch.explore, "Explore Content");
    SPECIFIC_FIELD(lang->muxlaunch.shutdown, "Shutdown");
    SPECIFIC_FIELD(lang->muxlaunch.reboot, "Reboot");
    SPECIFIC_FIELD(lang->muxlaunch.confirm_reboot, "Confirm Reboot");
    SPECIFIC_FIELD(lang->muxlaunch.confirm_shutdown, "Confirm Shutdown");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.app, "Apps");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.config, "Config");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.info, "Info");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.collection, "Collection");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.history, "History");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.explore, "Content");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.shutdown, "Shutdown");
    SPECIFIC_FIELD(lang->muxlaunch.abbr.reboot, "Reboot");
    SPECIFIC_FIELD(lang->muxlaunch.kiosk.error, "Kiosk configuration not found");
    SPECIFIC_FIELD(lang->muxlaunch.kiosk.process, "Processing kiosk configuration");
    SPECIFIC_FIELD(lang->muxlaunch.help.apps, "Various applications can be found and launched here");
    SPECIFIC_FIELD(lang->muxlaunch.help.config, "Various configurations can be changed here");
    SPECIFIC_FIELD(lang->muxlaunch.help.info, "Various information can be found and launched here");
    SPECIFIC_FIELD(lang->muxlaunch.help.collection, "Content added to collections can be found and launched here");
    SPECIFIC_FIELD(lang->muxlaunch.help.history, "Content previously launched can be found and launched here");
    SPECIFIC_FIELD(
        lang->muxlaunch.help.explore, "Content on storage devices (SD1/SD2/USB) can be found and launched here"
    );
    SPECIFIC_FIELD(lang->muxlaunch.help.shutdown, "Shut down your device safely");
    SPECIFIC_FIELD(lang->muxlaunch.help.reboot, "Reboot your device safely");

    // muxnetadv
    SPECIFIC_FIELD(lang->muxnetadv.title, "NETWORK SETTINGS");
    SPECIFIC_FIELD(lang->muxnetadv.monitor, "Connection Monitor");
    SPECIFIC_FIELD(lang->muxnetadv.boot, "Start Network on Boot");
    SPECIFIC_FIELD(lang->muxnetadv.wake, "Start Network on Wake");
    SPECIFIC_FIELD(lang->muxnetadv.compat, "Module Compatibility");
    SPECIFIC_FIELD(lang->muxnetadv.asyncload, "Module Async Load");
    SPECIFIC_FIELD(lang->muxnetadv.conretry, "Connection Retry");
    SPECIFIC_FIELD(lang->muxnetadv.wait, "Module Wait Timer");
    SPECIFIC_FIELD(lang->muxnetadv.modretry, "Module Retry");
    SPECIFIC_FIELD(
        lang->muxnetadv.help.monitor,
        "Enables periodic connectivity checks and triggers reconnection if network loss is detected"
    );
    SPECIFIC_FIELD(lang->muxnetadv.help.boot, "Enables network connection to be established automatically at boot");
    SPECIFIC_FIELD(
        lang->muxnetadv.help.wake, "Enables network connection to be re-established automatically upon suspend wake"
    );
    SPECIFIC_FIELD(
        lang->muxnetadv.help.compat, "Enable device compatibility with network module loading via the Linux "
                                     "kernel\n\nIncreases boot times moderately"
    );
    SPECIFIC_FIELD(
        lang->muxnetadv.help.async_load,
        "Enable the background handling of compatibility handling\n\nProvides faster boot times with "
        "compatibility enabled, disable this only if all else has failed"
    );
    SPECIFIC_FIELD(
        lang->muxnetadv.help.con_retry,
        "Adjusts how many retries the network connection script goes through before giving up"
    );
    SPECIFIC_FIELD(
        lang->muxnetadv.help.wait,
        "Adjusts the maximum amount of time waiting for the network interface to appear.\n\nWARNING:\nIf you enable "
        "Module Compatibility, it is not advisable to increase this setting as it will increase boot times!"
    );
    SPECIFIC_FIELD(
        lang->muxnetadv.help.mod_retry,
        "Adjusts the maximum amount of attempts at loading the network module with Module Compatibility "
        "enabled. Increase this setting if you are still unable to connect to wifi.\n\nIncreasing this "
        "setting may increase boot times importantly."
    );

    // muxnetproxy
    SPECIFIC_FIELD(lang->muxnetproxy.title, "PROXY SETTINGS");
    SPECIFIC_FIELD(lang->muxnetproxy.enabled, "Proxy Enabled");
    SPECIFIC_FIELD(lang->muxnetproxy.type, "Proxy Type");
    SPECIFIC_FIELD(lang->muxnetproxy.server, "Proxy Server");
    SPECIFIC_FIELD(lang->muxnetproxy.noproxy, "No Proxy");
    SPECIFIC_FIELD(lang->muxnetproxy.test, "Test Proxy");
    SPECIFIC_FIELD(lang->muxnetproxy.http, "HTTP");
    SPECIFIC_FIELD(lang->muxnetproxy.https, "HTTPS");
    SPECIFIC_FIELD(lang->muxnetproxy.socks5, "SOCKS5");
    SPECIFIC_FIELD(lang->muxnetproxy.testing, "Testing…");
    SPECIFIC_FIELD(lang->muxnetproxy.test_ok, "Proxy reachable");
    SPECIFIC_FIELD(lang->muxnetproxy.test_fail, "Proxy unreachable");
    SPECIFIC_FIELD(lang->muxnetproxy.no_server, "No proxy server configured");
    SPECIFIC_FIELD(lang->muxnetproxy.saved, "Proxy settings saved");
    SPECIFIC_FIELD(lang->muxnetproxy.reboot, "Reboot required to apply changes");
    SPECIFIC_FIELD(lang->muxnetproxy.help.enabled, "Enable or disable proxy routing for all outbound network traffic");
    SPECIFIC_FIELD(lang->muxnetproxy.help.type, "Select the proxy protocol type of either HTTP, HTTPS, or SOCKS5");
    SPECIFIC_FIELD(
        lang->muxnetproxy.help.server, "Enter the proxy server address in host:port format (e.g. 62.133.62.17:1082)"
    );
    SPECIFIC_FIELD(
        lang->muxnetproxy.help.no_proxy,
        "Comma separated list of hosts that bypass the proxy (e.g. localhost,127.0.0.1,::1)"
    );
    SPECIFIC_FIELD(
        lang->muxnetproxy.help.test, "Test connectivity through the configured proxy server using a live HTTP request"
    );

    // muxnetinfo
    SPECIFIC_FIELD(lang->muxnetinfo.title, "NETWORK DETAILS");
    SPECIFIC_FIELD(lang->muxnetinfo.hostname, "Hostname");
    SPECIFIC_FIELD(lang->muxnetinfo.mac, "MAC Address");
    SPECIFIC_FIELD(lang->muxnetinfo.ip, "IP Address");
    SPECIFIC_FIELD(lang->muxnetinfo.ssid, "Access Point");
    SPECIFIC_FIELD(lang->muxnetinfo.gateway, "Gateway");
    SPECIFIC_FIELD(lang->muxnetinfo.dns, "DNS");
    SPECIFIC_FIELD(lang->muxnetinfo.signal, "Signal");
    SPECIFIC_FIELD(lang->muxnetinfo.channel, "Channel");
    SPECIFIC_FIELD(lang->muxnetinfo.actraffic, "Accumulated Traffic");
    SPECIFIC_FIELD(lang->muxnetinfo.tptraffic, "Throughput Traffic");
    SPECIFIC_FIELD(lang->muxnetinfo.help.hostname, "The current hostname of this device");
    SPECIFIC_FIELD(lang->muxnetinfo.help.mac, "The unique hardware address of the network interface");
    SPECIFIC_FIELD(lang->muxnetinfo.help.ip, "The current IP address assigned to this device");
    SPECIFIC_FIELD(lang->muxnetinfo.help.ssid, "The name (SSID) of the connected Wi-Fi network");
    SPECIFIC_FIELD(lang->muxnetinfo.help.gateway, "The network gateway used to reach external networks");
    SPECIFIC_FIELD(lang->muxnetinfo.help.dns, "The DNS servers used to resolve domain names");
    SPECIFIC_FIELD(lang->muxnetinfo.help.signal, "The Wi-Fi signal strength expressed as a percentage");
    SPECIFIC_FIELD(lang->muxnetinfo.help.channel, "The Wi-Fi frequency and connected channel of the access point");
    SPECIFIC_FIELD(lang->muxnetinfo.help.ac_traffic, "The total data sent and received over the network");
    SPECIFIC_FIELD(lang->muxnetinfo.help.tp_traffic, "The current data sent and received over the network");
    SPECIFIC_FIELD(lang->muxnetinfo.error.edit, "Cannot edit if network is active!");
    SPECIFIC_FIELD(lang->muxnetinfo.error.change, "Cannot change if network is active!");
    SPECIFIC_FIELD(lang->muxnetinfo.save.host, "Saving Hostname");
    SPECIFIC_FIELD(lang->muxnetinfo.save.mac, "Changing MAC Address");
    SPECIFIC_FIELD(lang->muxnetinfo.report.success, "Network diagnostics exported");
    SPECIFIC_FIELD(lang->muxnetinfo.report.fail, "Error exporting network diagnostics");

    // muxnetprofile
    SPECIFIC_FIELD(lang->muxnetprofile.title, "WI-FI PROFILE");
    SPECIFIC_FIELD(lang->muxnetprofile.connect, "Connect");
    SPECIFIC_FIELD(lang->muxnetprofile.disconnect, "Disconnect");
    SPECIFIC_FIELD(lang->muxnetprofile.connected, "Connected");
    SPECIFIC_FIELD(lang->muxnetprofile.not_connected, "Not Connected");
    SPECIFIC_FIELD(lang->muxnetprofile.deny_modify, "Cannot modify while connected");
    SPECIFIC_FIELD(lang->muxnetprofile.deny_forget, "Cannot forget active network");
    SPECIFIC_FIELD(lang->muxnetprofile.save, "Changes Saved");
    SPECIFIC_FIELD(lang->muxnetprofile.dhcp, "DHCP");
    SPECIFIC_FIELD(lang->muxnetprofile.statc, "Static");
    SPECIFIC_FIELD(lang->muxnetprofile.subnet, "Subnet CIDR");
    SPECIFIC_FIELD(lang->muxnetprofile.profiles, "Profiles");
    SPECIFIC_FIELD(lang->muxnetprofile.connect_try, "Trying to Connect…");
    SPECIFIC_FIELD(lang->muxnetprofile.connect_deny, "Disconnect active network first");
    SPECIFIC_FIELD(lang->muxnetprofile.password, "Password");
    SPECIFIC_FIELD(lang->muxnetprofile.no_password, "No Password Detected…");
    SPECIFIC_FIELD(lang->muxnetprofile.dns, "DNS Server");
    SPECIFIC_FIELD(lang->muxnetprofile.address, "Device IP");
    SPECIFIC_FIELD(lang->muxnetprofile.encrypt_password, "Encrypting Password…");
    SPECIFIC_FIELD(lang->muxnetprofile.gateway, "Gateway IP");
    SPECIFIC_FIELD(lang->muxnetprofile.identifier, "Network Name");
    SPECIFIC_FIELD(lang->muxnetprofile.profile_name, "Profile Name");
    SPECIFIC_FIELD(lang->muxnetprofile.priority, "Priority");
    SPECIFIC_FIELD(lang->muxnetprofile.disabled, "Network Disabled");
    SPECIFIC_FIELD(lang->muxnetprofile.type, "Network Type");
    SPECIFIC_FIELD(lang->muxnetprofile.check, "Please check network settings");
    SPECIFIC_FIELD(lang->muxnetprofile.forget, "Forget");
    SPECIFIC_FIELD(lang->muxnetprofile.forget_confirm, "Forget this network?");
    SPECIFIC_FIELD(lang->muxnetprofile.status.associating, "Associating");
    SPECIFIC_FIELD(lang->muxnetprofile.status.authenticating, "Authenticating");
    SPECIFIC_FIELD(lang->muxnetprofile.status.waiting_ip, "Getting IP");
    SPECIFIC_FIELD(lang->muxnetprofile.status.validating, "Validating");
    SPECIFIC_FIELD(lang->muxnetprofile.status.invalid_password, "Invalid Password");
    SPECIFIC_FIELD(lang->muxnetprofile.status.ap_not_found, "Network Not Found");
    SPECIFIC_FIELD(lang->muxnetprofile.status.auth_timeout, "Auth Timeout");
    SPECIFIC_FIELD(lang->muxnetprofile.status.dhcp_failed, "DHCP Failure");
    SPECIFIC_FIELD(lang->muxnetprofile.status.link_timeout, "Link Timeout");
    SPECIFIC_FIELD(lang->muxnetprofile.status.wpa_start_failed, "AP Connect Error");
    SPECIFIC_FIELD(lang->muxnetprofile.help.type, "Toggle between DHCP and Static network types");
    SPECIFIC_FIELD(lang->muxnetprofile.help.password, "Enter the network password here (optional)");
    SPECIFIC_FIELD(lang->muxnetprofile.help.identifier, "Enter the name of the Wi-Fi network to connect to");
    SPECIFIC_FIELD(lang->muxnetprofile.help.profile_name, "Enter a name for this saved network profile");
    SPECIFIC_FIELD(
        lang->muxnetprofile.help.priority,
        "Set connection priority (0 highest, 9 lowest) when multiple saved networks are in range"
    );
    SPECIFIC_FIELD(lang->muxnetprofile.help.gateway, "Enter the network gateway address here (Static only)");
    SPECIFIC_FIELD(lang->muxnetprofile.help.subnet, "Enter the device Subnet (CIDR) number here (Static only)");
    SPECIFIC_FIELD(lang->muxnetprofile.help.address, "Enter the device IP address here (Static only)");
    SPECIFIC_FIELD(lang->muxnetprofile.help.dns, "Enter the device DNS address here (Static only)");
    SPECIFIC_FIELD(lang->muxnetprofile.help.connect, "Connect to the network using options entered above");

    // muxnetscan
    SPECIFIC_FIELD(lang->muxnetscan.title, "NETWORK SCAN");
    SPECIFIC_FIELD(lang->muxnetscan.scan, "Scanning for Wi-Fi Networks…");
    SPECIFIC_FIELD(lang->muxnetscan.none, "No Wi-Fi Networks Found");
    SPECIFIC_FIELD(lang->muxnetscan.help, "Detect, display and connect to available Wi-Fi networks");

    // muxnetwork
    SPECIFIC_FIELD(lang->muxnetwork.title, "WI-FI NETWORK");
    SPECIFIC_FIELD(lang->muxnetwork.none, "No Saved Networks Found");
    SPECIFIC_FIELD(lang->muxnetwork.connected, "Connected");
    SPECIFIC_FIELD(lang->muxnetwork.autom, "Auto");
    SPECIFIC_FIELD(lang->muxnetwork.manual, "Manual");
    SPECIFIC_FIELD(lang->muxnetwork.help, "Manage saved Wi-Fi networks and auto-connect preferences");

    // muxnews
    SPECIFIC_FIELD(lang->muxnews.title, "COMMUNITY NEWS");
    SPECIFIC_FIELD(lang->muxnews.none, "No News Found…");
    SPECIFIC_FIELD(lang->muxnews.download, "Downloading Community News…");
    SPECIFIC_FIELD(lang->muxnews.error, "Error Obtaining Community News");
    SPECIFIC_FIELD(lang->muxnews.open, "Downloading Post…");

    // muxoption
    SPECIFIC_FIELD(lang->muxoption.title_main, "CONTENT OPTIONS");
    SPECIFIC_FIELD(lang->muxoption.title_info, "CONTENT INFO");
    SPECIFIC_FIELD(lang->muxoption.name, "Content Name");
    SPECIFIC_FIELD(lang->muxoption.time, "Time Played");
    SPECIFIC_FIELD(lang->muxoption.launch, "Times Launched");
    SPECIFIC_FIELD(lang->muxoption.current, "Current");
    SPECIFIC_FIELD(lang->muxoption.core, "Content Core");
    SPECIFIC_FIELD(lang->muxoption.governor, "System Governor");
    SPECIFIC_FIELD(lang->muxoption.control, "Control Scheme");
    SPECIFIC_FIELD(lang->muxoption.retroarch, "Threaded Video");
    SPECIFIC_FIELD(lang->muxoption.colfilter, "Colour Filter");
    SPECIFIC_FIELD(lang->muxoption.shader, "Shader");
    SPECIFIC_FIELD(lang->muxoption.tag, "Content Tag");
    SPECIFIC_FIELD(lang->muxoption.remconfig, "Remove Config");
    SPECIFIC_FIELD(lang->muxoption.remcontent, "Removing Content Config…");
    SPECIFIC_FIELD(lang->muxoption.remdir, "Removing Directory Config…");
    SPECIFIC_FIELD(lang->muxoption.remcore, "Removing Core Config…");
    SPECIFIC_FIELD(lang->muxoption.none, "None");
    SPECIFIC_FIELD(lang->muxoption.not_assigned, "Not Assigned");
    SPECIFIC_FIELD(lang->muxoption.storage, "Storage");
    SPECIFIC_FIELD(lang->muxoption.folder, "Folder Path");
    SPECIFIC_FIELD(
        lang->muxoption.help.core, "Set the system core or external emulator for the selected content or directory"
    );
    SPECIFIC_FIELD(lang->muxoption.help.governor, "Set the CPU governor for the selected content or directory");
    SPECIFIC_FIELD(lang->muxoption.help.control, "Set the control scheme for the selected content or directory");
    SPECIFIC_FIELD(
        lang->muxoption.help.retro_arch,
        "Until RetroArch sorts their issue out with the threaded video option, you can toggle it here "
        "instead\n\nThis also may turn into a larger set of options in the future!"
    );
    SPECIFIC_FIELD(
        lang->muxoption.help.col_filter, "Select a colour filter that will change the display output of the "
                                         "running content. May not work with everything!"
    );
    SPECIFIC_FIELD(
        lang->muxoption.help.shader,
        "Select a shader that will affect the look and feel of the running content. May not work with everything!"
    );
    SPECIFIC_FIELD(lang->muxoption.help.tag, "Set the specific tag of the content selected to change the glyph");
    SPECIFIC_FIELD(
        lang->muxoption.help.rem_config,
        "Remove specific RetroArch configuration, just in case something goes horribly wrong"
    );
    SPECIFIC_FIELD(lang->muxoption.help.storage, "Displays which storage device the selected content is located on");
    SPECIFIC_FIELD(lang->muxoption.help.folder, "Shows the parent directory of the selected content");
    SPECIFIC_FIELD(lang->muxoption.help.name, "Displays the resolved or friendly name of the selected content");
    SPECIFIC_FIELD(lang->muxoption.help.time, "Shows the total accumulated play time for the selected content");
    SPECIFIC_FIELD(lang->muxoption.help.launch, "Displays how many times the selected content has been launched");

    // muxoverlay
    SPECIFIC_FIELD(lang->muxoverlay.title, "OVERLAY OPTIONS");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.top.left, "Top Left");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.top.middle, "Top Middle");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.top.right, "Top Right");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.centre.left, "Centre Left");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.centre.middle, "Centre Middle");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.centre.right, "Centre Right");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.bottom.left, "Bottom Left");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.bottom.middle, "Bottom Middle");
    SPECIFIC_FIELD(lang->muxoverlay.anchor.bottom.right, "Bottom Right");
    SPECIFIC_FIELD(lang->muxoverlay.scale.original, "None");
    SPECIFIC_FIELD(lang->muxoverlay.scale.fit, "Scale");
    SPECIFIC_FIELD(lang->muxoverlay.scale.stretch, "Stretch");
    SPECIFIC_FIELD(lang->muxoverlay.genalpha, "Content Base Transparency");
    SPECIFIC_FIELD(lang->muxoverlay.genanchor, "Content Base Position");
    SPECIFIC_FIELD(lang->muxoverlay.genscale, "Content Base Scale");
    SPECIFIC_FIELD(lang->muxoverlay.batalpha, "Low Battery Transparency");
    SPECIFIC_FIELD(lang->muxoverlay.batanchor, "Low Battery Position");
    SPECIFIC_FIELD(lang->muxoverlay.batscale, "Low Battery Scale");
    SPECIFIC_FIELD(lang->muxoverlay.volalpha, "Volume Transparency");
    SPECIFIC_FIELD(lang->muxoverlay.volanchor, "Volume Position");
    SPECIFIC_FIELD(lang->muxoverlay.volscale, "Volume Scale");
    SPECIFIC_FIELD(lang->muxoverlay.brialpha, "Brightness Transparency");
    SPECIFIC_FIELD(lang->muxoverlay.brianchor, "Brightness Position");
    SPECIFIC_FIELD(lang->muxoverlay.briscale, "Brightness Scale");
    SPECIFIC_FIELD(lang->muxoverlay.help.gen_alpha, "Controls the transparency of the base content overlay");
    SPECIFIC_FIELD(
        lang->muxoverlay.help.gen_anchor, "Sets the screen position where the base content overlay is anchored"
    );
    SPECIFIC_FIELD(lang->muxoverlay.help.gen_scale, "Controls how the base content overlay is scaled on the screen");
    SPECIFIC_FIELD(lang->muxoverlay.help.bat_alpha, "Controls the transparency of the low battery indicator overlay");
    SPECIFIC_FIELD(
        lang->muxoverlay.help.bat_anchor, "Sets the screen position where the low battery indicator is displayed"
    );
    SPECIFIC_FIELD(lang->muxoverlay.help.bat_scale, "Controls how the low battery indicator is scaled on the screen");
    SPECIFIC_FIELD(lang->muxoverlay.help.vol_alpha, "Controls the transparency of the volume indicator overlay");
    SPECIFIC_FIELD(
        lang->muxoverlay.help.vol_anchor, "Sets the screen position where the volume indicator is displayed"
    );
    SPECIFIC_FIELD(lang->muxoverlay.help.vol_scale, "Controls how the volume indicator is scaled on the screen");
    SPECIFIC_FIELD(lang->muxoverlay.help.bri_alpha, "Controls the transparency of the brightness indicator overlay");
    SPECIFIC_FIELD(
        lang->muxoverlay.help.bri_anchor, "Sets the screen position where the brightness indicator is displayed"
    );
    SPECIFIC_FIELD(lang->muxoverlay.help.bri_scale, "Controls how the brightness indicator is scaled on the screen");

    // muxpass
    SPECIFIC_FIELD(lang->muxpass.title, "PASSCODE");

    // muxpasscfg
    SPECIFIC_FIELD(lang->muxpasscfg.title, "PASSCODE SETTINGS");
    SPECIFIC_FIELD(lang->muxpasscfg.bootcode, "Boot Code");
    SPECIFIC_FIELD(lang->muxpasscfg.bootmsg, "Boot Message");
    SPECIFIC_FIELD(lang->muxpasscfg.launchcode, "Launch Code");
    SPECIFIC_FIELD(lang->muxpasscfg.launchmsg, "Launch Message");
    SPECIFIC_FIELD(lang->muxpasscfg.settingcode, "Settings Code");
    SPECIFIC_FIELD(lang->muxpasscfg.settingmsg, "Settings Message");
    SPECIFIC_FIELD(lang->muxpasscfg.safetycode, "Safety Code");
    SPECIFIC_FIELD(lang->muxpasscfg.saved, "Passcode saved");
    SPECIFIC_FIELD(lang->muxpasscfg.invalid, "Code must be up to 6 digits");
    SPECIFIC_FIELD(lang->muxpasscfg.help.boot_code, "6 digit code required at boot - set to 000000 to disable");
    SPECIFIC_FIELD(lang->muxpasscfg.help.boot_msg, "Message shown when the device first boots");
    SPECIFIC_FIELD(
        lang->muxpasscfg.help.launch_code, "6 digit code required before launching content - set to 000000 to disable"
    );
    SPECIFIC_FIELD(lang->muxpasscfg.help.launch_msg, "Message shown when launching content");
    SPECIFIC_FIELD(
        lang->muxpasscfg.help.setting_code, "6 digit code required to access settings - set to 000000 to disable"
    );
    SPECIFIC_FIELD(lang->muxpasscfg.help.setting_msg, "Message shown when accessing settings");
    SPECIFIC_FIELD(
        lang->muxpasscfg.help.safety_code, "Emergency recovery code that bypasses all locks - set to 000000 to disable"
    );

    // muxpicker
    SPECIFIC_FIELD(lang->muxpicker.custom, "CUSTOM PICKER");
    SPECIFIC_FIELD(lang->muxpicker.catalogue, "CATALOGUE PICKER");
    SPECIFIC_FIELD(lang->muxpicker.config, "CONFIG PICKER");
    SPECIFIC_FIELD(lang->muxpicker.theme, "THEME PICKER");
    SPECIFIC_FIELD(lang->muxpicker.theme_down, "Theme Download");
    SPECIFIC_FIELD(lang->muxpicker.invalid_ver, "Incompatible Theme Version Detected");
    SPECIFIC_FIELD(lang->muxpicker.invalid_res, "Incompatible Theme Resolution Detected");
    SPECIFIC_FIELD(lang->muxpicker.protected, "This theme is protected from deletion!");
    SPECIFIC_FIELD(lang->muxpicker.none.credit, "There are no attributed credits!");
    SPECIFIC_FIELD(lang->muxpicker.none.custom, "No Custom Packages Found");
    SPECIFIC_FIELD(lang->muxpicker.none.catalogue, "No Catalogue Packages Found");
    SPECIFIC_FIELD(lang->muxpicker.none.config, "No Configuration Packages Found");
    SPECIFIC_FIELD(lang->muxpicker.none.theme, "No Theme Packages Found");

    // muxplore
    SPECIFIC_FIELD(lang->muxplore.title, "EXPLORE");
    SPECIFIC_FIELD(lang->muxplore.none, "No Content Found…");
    SPECIFIC_FIELD(lang->muxplore.error.no_folder, "Folders cannot be added to collections");
    SPECIFIC_FIELD(lang->muxplore.error.no_core, "Content is not associated with system or core");
    SPECIFIC_FIELD(lang->muxplore.error.general, "Could not load content");

    // muxpower
    SPECIFIC_FIELD(lang->muxpower.title, "POWER SETTINGS");
    SPECIFIC_FIELD(lang->muxpower.low_battery, "Low Battery Indicator");
    SPECIFIC_FIELD(lang->muxpower.saver.type.title, "Screensaver Type");
    SPECIFIC_FIELD(lang->muxpower.saver.type.dvd, "DVD Bounce");
    SPECIFIC_FIELD(lang->muxpower.saver.type.star, "Starfield");
    SPECIFIC_FIELD(lang->muxpower.saver.type.matrix, "Digital Rain");
    SPECIFIC_FIELD(lang->muxpower.saver.type.firefly, "Fireflies");
    SPECIFIC_FIELD(lang->muxpower.saver.type.pulse, "Pulse Grid");
    SPECIFIC_FIELD(lang->muxpower.saver.type.trace, "Circuit Trace");
    SPECIFIC_FIELD(lang->muxpower.saver.type.constellation, "Constellation");
    SPECIFIC_FIELD(lang->muxpower.saver.type.mystify, "Mystify");
    SPECIFIC_FIELD(lang->muxpower.saver.type.maze, "Maze Runner");
    SPECIFIC_FIELD(lang->muxpower.saver.type.blockfall, "Block Fall");
    SPECIFIC_FIELD(lang->muxpower.saver.type.datetime, "Date and Time");
    SPECIFIC_FIELD(lang->muxpower.saver.type.video, "Video Wallpaper");
    SPECIFIC_FIELD(lang->muxpower.saver.type.slideshow, "Image Slideshow");
    SPECIFIC_FIELD(lang->muxpower.saver.speed.title, "Screensaver Speed");
    SPECIFIC_FIELD(lang->muxpower.saver.speed.crawl, "Crawl");
    SPECIFIC_FIELD(lang->muxpower.saver.speed.cruise, "Cruise");
    SPECIFIC_FIELD(lang->muxpower.saver.speed.fast, "Fast");
    SPECIFIC_FIELD(lang->muxpower.saver.speed.turbo, "Turbo");
    SPECIFIC_FIELD(lang->muxpower.idle.error, "Idle Display must be less than Idle Sleep");
    SPECIFIC_FIELD(lang->muxpower.idle.display, "Idle Input Display Timeout");
    SPECIFIC_FIELD(lang->muxpower.idle.sleep, "Idle Input Sleep Timeout");
    SPECIFIC_FIELD(lang->muxpower.idle.mute, "Mute on Display Timeout");
    SPECIFIC_FIELD(lang->muxpower.idle.t10_s, "10s");
    SPECIFIC_FIELD(lang->muxpower.idle.t30_s, "30s");
    SPECIFIC_FIELD(lang->muxpower.idle.t60_s, "60s");
    SPECIFIC_FIELD(lang->muxpower.idle.t2_m, "2m");
    SPECIFIC_FIELD(lang->muxpower.idle.t5_m, "5m");
    SPECIFIC_FIELD(lang->muxpower.idle.t10_m, "10m");
    SPECIFIC_FIELD(lang->muxpower.idle.t15_m, "15m");
    SPECIFIC_FIELD(lang->muxpower.idle.t30_m, "30m");
    SPECIFIC_FIELD(lang->muxpower.sleep.title, "Sleep Function");
    SPECIFIC_FIELD(lang->muxpower.sleep.instant, "Instant Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.suspend, "Sleep Until Wake");
    SPECIFIC_FIELD(lang->muxpower.sleep.t10_s, "Sleep 10s + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t30_s, "Sleep 30s + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t60_s, "Sleep 60s + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t2_m, "Sleep 2m + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t5_m, "Sleep 5m + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t10_m, "Sleep 10m + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t15_m, "Sleep 15m + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t30_m, "Sleep 30m + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.sleep.t60_m, "Sleep 60m + Shutdown");
    SPECIFIC_FIELD(lang->muxpower.gov.idle, "Idle Governor");
    SPECIFIC_FIELD(lang->muxpower.gov.dflt, "Default Governor");
    SPECIFIC_FIELD(
        lang->muxpower.help.idle_sleep, "Configure the time the device will sleep when no input is detected"
    );
    SPECIFIC_FIELD(
        lang->muxpower.help.idle_display, "Configure the time the screen will dim when no input is detected"
    );
    SPECIFIC_FIELD(lang->muxpower.help.idle_mute, "Toggle if the audio is muted when display is dimmed");
    SPECIFIC_FIELD(lang->muxpower.help.gov_idle, "Configure the frontend and device power governor on idle input");
    SPECIFIC_FIELD(lang->muxpower.help.gov_default, "Configure the default frontend and device power governor");
    SPECIFIC_FIELD(
        lang->muxpower.help.battery, "Configure when the red LED will display based on the current capacity percentage"
    );
    SPECIFIC_FIELD(lang->muxpower.help.shutdown, "Configure how the power button functions on short press");
    SPECIFIC_FIELD(
        lang->muxpower.help.saver_type,
        "Choose the frontend screensaver shown on Idle Display Timeout, or disable it entirely"
    );
    SPECIFIC_FIELD(lang->muxpower.help.saver_speed, "Adjust the speed of the screensaver");

    // muxraopt
    SPECIFIC_FIELD(lang->muxraopt.title, "THREADED VIDEO");
    SPECIFIC_FIELD(
        lang->muxraopt.help,
        "Until RetroArch sorts their issue out with the threaded video option, you can toggle it here "
        "instead\n\nThis also may turn into a larger set of options in the future!"
    );

    // muxremap
    SPECIFIC_FIELD(lang->muxremap.title, "Input Remap");
    SPECIFIC_FIELD(lang->muxremap.none, "No Controller Detected");
    SPECIFIC_FIELD(lang->muxremap.waiting, "Press Any Input…");
    SPECIFIC_FIELD(lang->muxremap.saved, "Mapping Saved");
    SPECIFIC_FIELD(lang->muxremap.input_label, "Controller");
    SPECIFIC_FIELD(lang->muxremap.layout_label, "Active Layout");
    SPECIFIC_FIELD(lang->muxremap.layout_retro, "Retro");
    SPECIFIC_FIELD(lang->muxremap.layout_modern, "Modern");

    // muxrgb
    SPECIFIC_FIELD(lang->muxrgb.title, "RGB LIGHTS");
    SPECIFIC_FIELD(lang->muxrgb.mode, "Mode");
    SPECIFIC_FIELD(lang->muxrgb.bright, "Brightness");
    SPECIFIC_FIELD(lang->muxrgb.speed, "Speed");
    SPECIFIC_FIELD(lang->muxrgb.breath_speed, "Breath Speed");
    SPECIFIC_FIELD(lang->muxrgb.colourl, "Left Stick Colour");
    SPECIFIC_FIELD(lang->muxrgb.colourr, "Right Stick Colour");
    SPECIFIC_FIELD(lang->muxrgb.colourm, "Middle Zone Colour");
    SPECIFIC_FIELD(lang->muxrgb.colourf1, "Front Zone 1 Colour");
    SPECIFIC_FIELD(lang->muxrgb.colourf2, "Front Zone 2 Colour");
    SPECIFIC_FIELD(lang->muxrgb.combo, "Colour Combo");
    SPECIFIC_FIELD(lang->muxrgb.backend, "Backend");
    SPECIFIC_FIELD(lang->muxrgb.same_as_l, "Same as Main");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.off, "Off");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.statc, "Static");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.breathing, "Breathing");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.colour_cycle, "Colour Cycle");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.rainbow, "Rainbow");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.stick_follow, "Stick Follow");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.preset_combo, "Preset Combo");
    SPECIFIC_FIELD(lang->muxrgb.mode_name.theme_supplied, "Theme Supplied");
    SPECIFIC_FIELD(lang->muxrgb.breath_speed_name.fast, "Fast");
    SPECIFIC_FIELD(lang->muxrgb.breath_speed_name.medium, "Medium");
    SPECIFIC_FIELD(lang->muxrgb.breath_speed_name.slow, "Slow");
    SPECIFIC_FIELD(lang->muxrgb.stick.primary, "Primary Colour");
    SPECIFIC_FIELD(lang->muxrgb.stick.secondary, "Secondary Colour");
    SPECIFIC_FIELD(lang->muxrgb.stick.single, "Stick Colour");
    SPECIFIC_FIELD(lang->muxrgb.stick.both, "Both Sticks");
    SPECIFIC_FIELD(lang->muxrgb.backend_name.autom, "Auto");
    SPECIFIC_FIELD(lang->muxrgb.backend_name.sysfs, "SysFS");
    SPECIFIC_FIELD(lang->muxrgb.backend_name.serial, "Serial");
    SPECIFIC_FIELD(lang->muxrgb.backend_name.joypad, "Joypad");
    SPECIFIC_FIELD(lang->muxrgb.help.mode, "The animation mode driving the RGB lights");
    SPECIFIC_FIELD(lang->muxrgb.help.bright, "Overall brightness of the RGB lights, from 1 to 10");
    SPECIFIC_FIELD(lang->muxrgb.help.breath_speed, "How quickly the breathing animation pulses");
    SPECIFIC_FIELD(lang->muxrgb.help.colour_l, "Primary colour used by single-colour modes");
    SPECIFIC_FIELD(
        lang->muxrgb.help.colour_r, "Right stick colour override\n\nSelect 'Same as Main' to follow the primary colour"
    );
    SPECIFIC_FIELD(
        lang->muxrgb.help.colour_m, "Middle zone colour override\n\nSelect 'Same as Main' to follow the primary colour"
    );
    SPECIFIC_FIELD(
        lang->muxrgb.help.colour_f1,
        "Front zone 1 colour override\n\nSelect 'Same as Main' to follow the primary colour"
    );
    SPECIFIC_FIELD(
        lang->muxrgb.help.colour_f2,
        "Front zone 2 colour override\n\nSelect 'Same as Main' to follow the primary colour"
    );
    SPECIFIC_FIELD(lang->muxrgb.help.combo, "Two-colour preset used by the Preset Combo mode");
    SPECIFIC_FIELD(
        lang->muxrgb.help.backend, "Driver backend used to talk to the RGB hardware\n\nLeave on Auto unless "
                                   "you know your device prefers a specific backend"
    );

    // muxrtc
    SPECIFIC_FIELD(lang->muxrtc.title, "DATE AND TIME");
    SPECIFIC_FIELD(lang->muxrtc.day, "Day");
    SPECIFIC_FIELD(lang->muxrtc.month, "Month");
    SPECIFIC_FIELD(lang->muxrtc.year, "Year");
    SPECIFIC_FIELD(lang->muxrtc.hour, "Hour");
    SPECIFIC_FIELD(lang->muxrtc.minute, "Minute");
    SPECIFIC_FIELD(lang->muxrtc.timezone, "Set Timezone");
    SPECIFIC_FIELD(lang->muxrtc.notation, "Time Notation");
    SPECIFIC_FIELD(lang->muxrtc.f_12_hr, "12 Hour");
    SPECIFIC_FIELD(lang->muxrtc.f_24_hr, "24 Hour");
    SPECIFIC_FIELD(lang->muxrtc.f_dd_mm_12, "DD/MM 12 Hour");
    SPECIFIC_FIELD(lang->muxrtc.f_dd_mm_24, "DD/MM 24 Hour");
    SPECIFIC_FIELD(lang->muxrtc.f_mm_dd_12, "MM/DD 12 Hour");
    SPECIFIC_FIELD(lang->muxrtc.f_mm_dd_24, "MM/DD 24 Hour");
    SPECIFIC_FIELD(lang->muxrtc.f_custom, "Custom");
    SPECIFIC_FIELD(lang->muxrtc.custom, "Custom Format");
    SPECIFIC_FIELD(lang->muxrtc.help, "Change your current date, time, and timezone");

    // muxsearch
    SPECIFIC_FIELD(lang->muxsearch.title, "SEARCH CONTENT");
    SPECIFIC_FIELD(lang->muxsearch.global, "Search Global");
    SPECIFIC_FIELD(lang->muxsearch.local, "Search Local");
    SPECIFIC_FIELD(lang->muxsearch.lookup, "Lookup");
    SPECIFIC_FIELD(lang->muxsearch.search, "Searching…");
    SPECIFIC_FIELD(lang->muxsearch.error, "Lookup has to be 3 characters or more!");
    SPECIFIC_FIELD(lang->muxsearch.help.search_global, "Search all current active storage devices");
    SPECIFIC_FIELD(lang->muxsearch.help.search_local, "Search within the current selected folder and folders within");
    SPECIFIC_FIELD(lang->muxsearch.help.lookup, "Enter in the name of the content you are looking for");

    // muxshader
    SPECIFIC_FIELD(lang->muxshader.title, "SHADERS");
    SPECIFIC_FIELD(lang->muxshader.help, "Change the shader of your current selected content");
    SPECIFIC_FIELD(lang->muxshader.none, "No Shaders Found…");

    // muxshot
    SPECIFIC_FIELD(lang->muxshot.title, "SCREENSHOTS");
    SPECIFIC_FIELD(lang->muxshot.help, "View your current screenshots");
    SPECIFIC_FIELD(lang->muxshot.none, "No Screenshots Found");

    // muxsort
    SPECIFIC_FIELD(lang->muxsort.title, "SORTING PRIORITY");
    SPECIFIC_FIELD(lang->muxsort.dflt, "Default");
    SPECIFIC_FIELD(lang->muxsort.collection, "Collection");
    SPECIFIC_FIELD(lang->muxsort.history, "History");
    SPECIFIC_FIELD(lang->muxsort.abandoned, "Abandoned");
    SPECIFIC_FIELD(lang->muxsort.alternate, "Alternate");
    SPECIFIC_FIELD(lang->muxsort.backlog, "Backlog");
    SPECIFIC_FIELD(lang->muxsort.complected, "Complected");
    SPECIFIC_FIELD(lang->muxsort.completed, "Completed");
    SPECIFIC_FIELD(lang->muxsort.cursed, "Cursed");
    SPECIFIC_FIELD(lang->muxsort.experimental, "Experimental");
    SPECIFIC_FIELD(lang->muxsort.homebrew, "Homebrew");
    SPECIFIC_FIELD(lang->muxsort.inprogress, "Inprogress");
    SPECIFIC_FIELD(lang->muxsort.patched, "Patched");
    SPECIFIC_FIELD(lang->muxsort.replay, "Replay");
    SPECIFIC_FIELD(lang->muxsort.romhack, "Romhack");
    SPECIFIC_FIELD(lang->muxsort.translated, "Translated");
    SPECIFIC_FIELD(
        lang->muxsort.help.dflt,
        "Set Content Explorer sorting level for Default items (100 top of list 1 bottom of list)"
    );
    SPECIFIC_FIELD(
        lang->muxsort.help.collection,
        "Set Content Explorer sorting level for Collection items (100 top of list 1 bottom of list)"
    );
    SPECIFIC_FIELD(
        lang->muxsort.help.history,
        "Set Content Explorer sorting level for History items (100 top of list 1 bottom of list)"
    );
    SPECIFIC_FIELD(
        lang->muxsort.help.tag,
        "Set Content Explorer sorting level for items tagged as %s (100 top of list 1 bottom of list)"
    );

    // muxspace
    SPECIFIC_FIELD(lang->muxspace.title, "STORAGE SPACE");
    SPECIFIC_FIELD(lang->muxspace.help, "View the current used space of the mounted storage devices");
    SPECIFIC_FIELD(lang->muxspace.primary, "Primary Storage");
    SPECIFIC_FIELD(lang->muxspace.secondary, "Secondary Storage");
    SPECIFIC_FIELD(lang->muxspace.external, "External Storage");
    SPECIFIC_FIELD(lang->muxspace.system, "System Storage");
    SPECIFIC_FIELD(lang->muxspace.detail.filesystem, "Filesystem");
    SPECIFIC_FIELD(lang->muxspace.detail.quality_check, "Quality Check");
    SPECIFIC_FIELD(lang->muxspace.detail.manufacturer, "Manufacturer");
    SPECIFIC_FIELD(lang->muxspace.detail.model, "Model");
    SPECIFIC_FIELD(lang->muxspace.detail.date, "Date");
    SPECIFIC_FIELD(lang->muxspace.quality.genuine, "Genuine");
    SPECIFIC_FIELD(lang->muxspace.quality.likely_genuine, "Likely Genuine");
    SPECIFIC_FIELD(lang->muxspace.quality.suspicious, "Suspicious");
    SPECIFIC_FIELD(lang->muxspace.quality.suspected_fake, "Suspected Fake");
    SPECIFIC_FIELD(lang->muxspace.quality.fake, "Fake");
    SPECIFIC_FIELD(lang->muxspace.quality.trash, "Trash");

    // muxstorage
    SPECIFIC_FIELD(lang->muxstorage.title, "STORAGE");
    SPECIFIC_FIELD(lang->muxstorage.apps, "Applications");
    SPECIFIC_FIELD(lang->muxstorage.bios, "System BIOS");
    SPECIFIC_FIELD(lang->muxstorage.catalogue, "Metadata Catalogue");
    SPECIFIC_FIELD(lang->muxstorage.collection, "Content Collection");
    SPECIFIC_FIELD(lang->muxstorage.history, "History");
    SPECIFIC_FIELD(lang->muxstorage.init, "User Init Scripts");
    SPECIFIC_FIELD(lang->muxstorage.music, "Background Music");
    SPECIFIC_FIELD(lang->muxstorage.name, "Friendly Name System");
    SPECIFIC_FIELD(lang->muxstorage.network, "Network Profiles");
    SPECIFIC_FIELD(lang->muxstorage.package, "Custom Packages");
    SPECIFIC_FIELD(lang->muxstorage.save, "Save Games + Save States");
    SPECIFIC_FIELD(lang->muxstorage.screenshot, "Screenshots");
    SPECIFIC_FIELD(lang->muxstorage.syncthing, "Syncthing Configs");
    SPECIFIC_FIELD(lang->muxstorage.theme, "Themes");
    SPECIFIC_FIELD(lang->muxstorage.track, "Activity Tracker");
    SPECIFIC_FIELD(lang->muxstorage.help.apps, "Location of installed applications");
    SPECIFIC_FIELD(lang->muxstorage.help.bios, "Location of system BIOS files");
    SPECIFIC_FIELD(lang->muxstorage.help.catalogue, "Location of content images and text");
    SPECIFIC_FIELD(lang->muxstorage.help.collection, "Location of content collection");
    SPECIFIC_FIELD(lang->muxstorage.help.history, "Location of history");
    SPECIFIC_FIELD(lang->muxstorage.help.init, "Location of User Initialisation scripts");
    SPECIFIC_FIELD(lang->muxstorage.help.music, "Location of background music");
    SPECIFIC_FIELD(lang->muxstorage.help.name, "Location of friendly name configurations");
    SPECIFIC_FIELD(lang->muxstorage.help.network, "Location of Network Profiles");
    SPECIFIC_FIELD(lang->muxstorage.help.package, "Location of custom packages");
    SPECIFIC_FIELD(lang->muxstorage.help.save, "Location of save states and files");
    SPECIFIC_FIELD(lang->muxstorage.help.screenshot, "Location of screenshots");
    SPECIFIC_FIELD(lang->muxstorage.help.syncthing, "Location of Syncthing configurations");
    SPECIFIC_FIELD(lang->muxstorage.help.theme, "Location of themes");
    SPECIFIC_FIELD(lang->muxstorage.help.track, "Location of activity tracker");

    // muxsysinfo
    SPECIFIC_FIELD(lang->muxsysinfo.title, "SYSTEM DETAILS");
    SPECIFIC_FIELD(lang->muxsysinfo.version, "Version");
    SPECIFIC_FIELD(lang->muxsysinfo.build, "Build ID");
    SPECIFIC_FIELD(lang->muxsysinfo.device, "Device Type");
    SPECIFIC_FIELD(lang->muxsysinfo.kernel, "Linux Kernel");
    SPECIFIC_FIELD(lang->muxsysinfo.arch, "Architecture");
    SPECIFIC_FIELD(lang->muxsysinfo.uptime, "System Uptime");
    SPECIFIC_FIELD(lang->muxsysinfo.boot_time, "Boot Time");
    SPECIFIC_FIELD(lang->muxsysinfo.load_avg, "Load Average");
    SPECIFIC_FIELD(lang->muxsysinfo.memory.info, "System Memory");
    SPECIFIC_FIELD(lang->muxsysinfo.memory.drop, "Memory Cache Dropped");
    SPECIFIC_FIELD(lang->muxsysinfo.swap, "Swap Memory");
    SPECIFIC_FIELD(lang->muxsysinfo.temp, "Temperature");
    SPECIFIC_FIELD(lang->muxsysinfo.reload, "Reload Frontend");
    SPECIFIC_FIELD(lang->muxsysinfo.reload_run, "Reloading Frontend…");
    SPECIFIC_FIELD(lang->muxsysinfo.cpu.info, "CPU Information");
    SPECIFIC_FIELD(lang->muxsysinfo.cpu.speed, "CPU Speed");
    SPECIFIC_FIELD(lang->muxsysinfo.cpu.governor, "CPU Governor");
    SPECIFIC_FIELD(
        lang->muxsysinfo.warn,
        "Changing your device functions may cause unexpected behaviour or prevent the system from booting!"
    );
    SPECIFIC_FIELD(lang->muxsysinfo.help.version, "The current version of MustardOS running on the device");
    SPECIFIC_FIELD(lang->muxsysinfo.help.build, "The current build ID of MustardOS running on the device");
    SPECIFIC_FIELD(lang->muxsysinfo.help.device, "The current device type detected and configured");
    SPECIFIC_FIELD(lang->muxsysinfo.help.kernel, "The current Linux kernel");
    SPECIFIC_FIELD(lang->muxsysinfo.help.arch, "The CPU instruction set architecture reported by the kernel");
    SPECIFIC_FIELD(lang->muxsysinfo.help.uptime, "The current running time of the system");
    SPECIFIC_FIELD(lang->muxsysinfo.help.boot_time, "The date and time the system was last booted");
    SPECIFIC_FIELD(
        lang->muxsysinfo.help.load_avg, "Average number of runnable processes over the last 1, 5, and 15 minutes"
    );
    SPECIFIC_FIELD(lang->muxsysinfo.help.memory, "The current, and total, device memory usage of the device");
    SPECIFIC_FIELD(lang->muxsysinfo.help.swap, "The current, and total, swap memory usage of the device");
    SPECIFIC_FIELD(lang->muxsysinfo.help.temp, "The current detected temperature of the device");
    SPECIFIC_FIELD(
        lang->muxsysinfo.help.reload, "Reload the current frontend configuration values if changed elsewhere"
    );
    SPECIFIC_FIELD(lang->muxsysinfo.help.cpu, "The detected CPU type of the device");
    SPECIFIC_FIELD(lang->muxsysinfo.help.speed, "The current CPU frequency of the device");
    SPECIFIC_FIELD(lang->muxsysinfo.help.governor, "The current running governor of the device");

    // muxtag
    SPECIFIC_FIELD(lang->muxtag.title, "TAG");
    SPECIFIC_FIELD(lang->muxtag.help, "Change the tag of your current selected content to make it stand out");
    SPECIFIC_FIELD(lang->muxtag.none, "No Tags Found…");

    // muxtask
    SPECIFIC_FIELD(lang->muxtask.title, "TASK TOOLKIT");
    SPECIFIC_FIELD(lang->muxtask.none, "No Tasks Found");

    // muxtester
    SPECIFIC_FIELD(lang->muxtester.title, "INPUT TESTER");
    SPECIFIC_FIELD(lang->muxtester.quit, "Press DOWN + B to finish testing");
    SPECIFIC_FIELD(lang->muxtester.quit_alt, "Press DOWN + A to finish testing");

    // muxthemedown
    SPECIFIC_FIELD(lang->muxthemedown.title, "THEME DOWNLOAD");
    SPECIFIC_FIELD(lang->muxthemedown.theme_removed, "Theme Removed");
    SPECIFIC_FIELD(lang->muxthemedown.theme_extracting, "Theme Extraction In Process");
    SPECIFIC_FIELD(lang->muxthemedown.none, "No Content Found…");
    SPECIFIC_FIELD(lang->muxthemedown.download, "Download");
    SPECIFIC_FIELD(lang->muxthemedown.down.theme, "Downloading Theme");
    SPECIFIC_FIELD(lang->muxthemedown.down.data, "Downloading Theme Data");
    SPECIFIC_FIELD(lang->muxthemedown.down.preview, "Downloading Theme Previews");
    SPECIFIC_FIELD(lang->muxthemedown.remove, "Remove");
    SPECIFIC_FIELD(lang->muxthemedown.error_get_data, "Error Retrieving Theme Data");

    // muxthemefilter
    SPECIFIC_FIELD(lang->muxthemefilter.title, "THEME FILTER");
    SPECIFIC_FIELD(lang->muxthemefilter.all_themes, "Theme Compatibility");
    SPECIFIC_FIELD(lang->muxthemefilter.compat.device, "Device");
    SPECIFIC_FIELD(lang->muxthemefilter.compat.all, "All");
    SPECIFIC_FIELD(lang->muxthemefilter.grid, "Grid");
    SPECIFIC_FIELD(lang->muxthemefilter.hdmi, "HDMI");
    SPECIFIC_FIELD(lang->muxthemefilter.language, "Language");
    SPECIFIC_FIELD(lang->muxthemefilter.lookup, "Lookup");
    SPECIFIC_FIELD(
        lang->muxthemefilter.help.all_themes,
        "Filter to themes for this device or all themes. All themes can work on any device but if the theme "
        "does not implement your devices resolution it will be letterboxed."
    );
    SPECIFIC_FIELD(
        lang->muxthemefilter.help.grid, "Filter to themes that support displaying content folders in a tile layout"
    );
    SPECIFIC_FIELD(
        lang->muxthemefilter.help.hdmi,
        "Filter to themes that support HDMI resolution 1280x720. Themes will still work on HDMI without "
        "support for 1280x720 but content will be letterboxed."
    );
    SPECIFIC_FIELD(
        lang->muxthemefilter.help.language,
        "Filter to themes that let MustardOS handle translating text. Themes that use static images for the "
        "main menu will be filtered out."
    );
    SPECIFIC_FIELD(lang->muxthemefilter.help.lookup, "Filter to theme with a name containing lookup text.");

    // muxthemeopt
    SPECIFIC_FIELD(lang->muxthemeopt.title, "THEME OPTIONS");
    SPECIFIC_FIELD(lang->muxthemeopt.header_height, "Header Height");
    SPECIFIC_FIELD(lang->muxthemeopt.footer_height, "Footer Height");
    SPECIFIC_FIELD(lang->muxthemeopt.content_item_count, "Item Count");
    SPECIFIC_FIELD(lang->muxthemeopt.glyph_list, "List Glyph Size");
    SPECIFIC_FIELD(lang->muxthemeopt.glyph_footer, "Footer Glyph Size");
    SPECIFIC_FIELD(lang->muxthemeopt.glyph_header, "Header Glyph Size");
    SPECIFIC_FIELD(lang->muxthemeopt.glyph_grid, "Grid Glyph Size");
    SPECIFIC_FIELD(lang->muxthemeopt.size_default, "Default");
    SPECIFIC_FIELD(lang->muxthemeopt.glyph_auto, "Auto");
    SPECIFIC_FIELD(lang->muxthemeopt.glyph_native, "Native");
    SPECIFIC_FIELD(lang->muxthemeopt.label_width, "Label Width");
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.header_height,
        "Override the theme header bar height in pixels (Default uses the theme value)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.footer_height,
        "Override the theme footer bar height in pixels (Default uses the theme value)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.content_item_count,
        "Override the number of visible list items (0 uses the theme default)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.glyph_list,
        "Override the list glyph render size in pixels (Default uses the theme value, Auto fits to item "
        "height, Native uses the actual size)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.glyph_footer,
        "Override the footer glyph render size in pixels (Default uses the theme value, Auto fits to item "
        "height, Native uses the actual size)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.glyph_header,
        "Override the header glyph render size in pixels (Default uses the theme value, Auto fits to header "
        "height, Native uses the actual size)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.glyph_grid,
        "Override the grid glyph render size in pixels (Default uses the theme value, Auto fits to the "
        "cell, Native uses the actual size)"
    );
    SPECIFIC_FIELD(
        lang->muxthemeopt.help.label_width, "Override the maximum width of the left side option label as a "
                                            "percentage of content width (Default uses the theme value)"
    );

    // muxtimezone
    SPECIFIC_FIELD(lang->muxtimezone.title, "TIMEZONE");
    SPECIFIC_FIELD(lang->muxtimezone.none, "No Timezones Found…");
    SPECIFIC_FIELD(lang->muxtimezone.save, "Saving Timezone");
    SPECIFIC_FIELD(lang->muxtimezone.help, "Select your preferred timezone");

    // muxtweakadv
    SPECIFIC_FIELD(lang->muxtweakadv.title, "ADVANCED SETTINGS");
    SPECIFIC_FIELD(lang->muxtweakadv.accelerate, "Menu Acceleration");
    SPECIFIC_FIELD(lang->muxtweakadv.repeatdelay, "Menu Repeat Delay");
    SPECIFIC_FIELD(lang->muxtweakadv.thermal, "Thermal Zone Control");
    SPECIFIC_FIELD(lang->muxtweakadv.led, "LED During Play");
    SPECIFIC_FIELD(lang->muxtweakadv.randomtheme, "Random Theme on Boot");
    SPECIFIC_FIELD(lang->muxtweakadv.retrowait, "RetroArch Network Wait");
    SPECIFIC_FIELD(lang->muxtweakadv.retrofree, "RetroArch Config Freedom");
    SPECIFIC_FIELD(lang->muxtweakadv.retrocache, "RetroArch Startup Cache");
    SPECIFIC_FIELD(lang->muxtweakadv.activity, "Activity Tracker");
    SPECIFIC_FIELD(lang->muxtweakadv.verbose, "Verbose Messages");
    SPECIFIC_FIELD(lang->muxtweakadv.debuglog, "Debug Logging");
    SPECIFIC_FIELD(lang->muxtweakadv.userinit, "User Init Scripts");
    SPECIFIC_FIELD(lang->muxtweakadv.dpadswap, "DPAD Swap Function");
    SPECIFIC_FIELD(lang->muxtweakadv.overdrive, "Audio Overdrive");
    SPECIFIC_FIELD(lang->muxtweakadv.swapfile, "System Swapfile");
    SPECIFIC_FIELD(lang->muxtweakadv.zramfile, "System ZRam");
    SPECIFIC_FIELD(lang->muxtweakadv.lidswitch, "Device Lid Switch");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.title, "Menu Navigation");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.dpad, "DPAD Only");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.ls, "L Stick Only");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.rs, "R Stick Only");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.dpad_ls, "DPAD + L Stick");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.dpad_rs, "DPAD + R Stick");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.dpad_ls_rs, "DPAD + L/R Stick");
    SPECIFIC_FIELD(lang->muxtweakadv.sticknav.ls_rs, "L/R Stick");
    SPECIFIC_FIELD(lang->muxtweakadv.volume.title, "Volume On Boot");
    SPECIFIC_FIELD(lang->muxtweakadv.volume.silent, "Silent");
    SPECIFIC_FIELD(lang->muxtweakadv.volume.soft, "Soft");
    SPECIFIC_FIELD(lang->muxtweakadv.volume.loud, "Loud");
    SPECIFIC_FIELD(lang->muxtweakadv.brightness.title, "Brightness On Boot");
    SPECIFIC_FIELD(lang->muxtweakadv.brightness.low, "Low");
    SPECIFIC_FIELD(lang->muxtweakadv.brightness.medium, "Medium");
    SPECIFIC_FIELD(lang->muxtweakadv.brightness.high, "High");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.title, "Device Rumble");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.st, "Startup");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.sh, "Shutdown");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.sl, "Sleep");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.stsh, "Startup + Shutdown");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.stsl, "Startup + Sleep");
    SPECIFIC_FIELD(lang->muxtweakadv.rumble.shsl, "Shutdown + Sleep");
    SPECIFIC_FIELD(lang->muxtweakadv.dispsuspend, "Display Suspend");
    SPECIFIC_FIELD(lang->muxtweakadv.stageoverlay, "Stage Overlay");
    SPECIFIC_FIELD(lang->muxtweakadv.secondpart, "Secondary Partition");
    SPECIFIC_FIELD(lang->muxtweakadv.usbpart, "External Partition");
    SPECIFIC_FIELD(lang->muxtweakadv.incbright, "Brightness Increment");
    SPECIFIC_FIELD(lang->muxtweakadv.incvolume, "Volume Increment");
    SPECIFIC_FIELD(lang->muxtweakadv.maxgpu, "GPU Performance Mode");
    SPECIFIC_FIELD(lang->muxtweakadv.doublebuffer, "Double Buffering");
    SPECIFIC_FIELD(lang->muxtweakadv.audioready, "Audio Subsystem Wait");
    SPECIFIC_FIELD(lang->muxtweakadv.audioswap, "Audio Reverse");
    SPECIFIC_FIELD(lang->muxtweakadv.audiosuspend, "Audio Suspend");
    SPECIFIC_FIELD(lang->muxtweakadv.btscantimeout, "Bluetooth Scan Timeout");
    SPECIFIC_FIELD(lang->muxtweakadv.seconds, "Seconds");
    SPECIFIC_FIELD(lang->muxtweakadv.trustmodify, "Trust Modifications");
    SPECIFIC_FIELD(lang->muxtweakadv.trustpower, "Trust Power Choice");
    SPECIFIC_FIELD(lang->muxtweakadv.trustremove, "Trust Removals");
    SPECIFIC_FIELD(lang->muxtweakadv.usbfunction, "USB Function");
    SPECIFIC_FIELD(lang->muxtweakadv.box_art_pad_div, "Box Art Padding Divisor");
    SPECIFIC_FIELD(lang->muxtweakadv.adb, "Android Debug Bridge");
    SPECIFIC_FIELD(lang->muxtweakadv.mtp, "Media Transfer Protocol");
    SPECIFIC_FIELD(lang->muxtweakadv.help.accelerate, "Adjust the rate of speed when holding navigation keys down");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.repeat_delay,
        "Adjust amount of time button must be held before it begins to repeat the button action"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.thermal,
        "Toggle the system ability to automatically shut the device down due high temperature"
    );
    SPECIFIC_FIELD(lang->muxtweakadv.help.led, "Toggle the power LED during content launch");
    SPECIFIC_FIELD(lang->muxtweakadv.help.random_theme, "Change the default theme used for the next device launch");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.retro_wait,
        "Toggle a delayed start of RetroArch until a network connection is established"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.retro_free, "Toggle the forced settings MustardOS places on RetroArch configurations"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.retro_cache, "Toggle the startup cache of RetroArch. This will increase boot "
                                            "by ~3s but will start RetroArch a bit faster on first runs"
    );
    SPECIFIC_FIELD(lang->muxtweakadv.help.activity, "Toggle the content activity tracker");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.verbose, "Toggle startup and shutdown verbose messages used for debugging faults"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.debug_log,
        "Toggle additional debug logging written to the internal log directory for diagnostic purposes only"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.user_init,
        "Toggle the functionality of the user initialisation scripts on device startup"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.dpad_swap, "Toggle the functionality of the power button to switch DPAD mode"
    );
    SPECIFIC_FIELD(lang->muxtweakadv.help.overdrive, "Toggle the audio overdrive moving it from 100% to 200%");
    SPECIFIC_FIELD(lang->muxtweakadv.help.swapfile, "Adjust the system swapfile if required by certain content");
    SPECIFIC_FIELD(lang->muxtweakadv.help.zramfile, "Adjust the system zram if required by certain content");
    SPECIFIC_FIELD(lang->muxtweakadv.help.lid_switch, "Toggle the lid switch functionality for the device");
    SPECIFIC_FIELD(lang->muxtweakadv.help.rumble, "Toggle vibration for device startup, sleep, and shutdown");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.brightness,
        "Change the default brightness level that the device will use each time it starts up"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.volume, "Change the default audio level that the device will use each time it starts up"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.stick_nav, "Change how you navigate using the DPAD and Analogue Sticks on the device"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.disp_suspend,
        "Toggle the device display suspend function, however some displays will not like this enabled"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.stage_overlay,
        "Toggle the stage overlay system used to display brightness, volume, and battery notifications"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.second_part, "Change the partition number requested upon secondary storage mount"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.usb_part, "Change the partition number requested upon external storage mount"
    );
    SPECIFIC_FIELD(lang->muxtweakadv.help.inc_bright, "Change the level of brightness incrementation when adjusting");
    SPECIFIC_FIELD(lang->muxtweakadv.help.inc_volume, "Change the level of volume incrementation when adjusting");
    SPECIFIC_FIELD(lang->muxtweakadv.help.max_gpu, "Push the onboard GPU to the maximum frequency at all times");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.double_buffer,
        "Allocate a second fullscreen draw buffer. Uses more memory and is mainly for debugging\n\nLeave "
        "disabled unless comparing rendering behaviour!\n\nTakes effect on next launch"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.audio_ready,
        "Toggle if the device will wait for the audio subsystem to initialise during boot"
    );
    SPECIFIC_FIELD(lang->muxtweakadv.help.audio_swap, "Toggle the swap of left and right channels of audio");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.audio_suspend,
        "Toggle if PipeWire will suspend the audio device when idle to reduce power consumption"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.bt_scan_timeout, "Adjust the duration the device will scan for nearby Bluetooth devices"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.trust_modify,
        "Skip the unsaved changes dialogue and save immediately when leaving a settings module"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.trust_power, "Skip the confirmation dialogue when choosing to reboot or shut down"
    );
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.trust_remove,
        "Skip the removal confirmation dialogue when using X to remove content or reset settings"
    );
    SPECIFIC_FIELD(lang->muxtweakadv.help.usb_function, "Toggle between ADB and MTP USB functionality");
    SPECIFIC_FIELD(
        lang->muxtweakadv.help.box_art_pad_div, "Divisor used to scale the box art padding percentage into pixels"
    );

    // muxtweakgen
    SPECIFIC_FIELD(lang->muxtweakgen.title, "GENERAL SETTINGS");
    SPECIFIC_FIELD(lang->muxtweakgen.rtc, "Date and Time");
    SPECIFIC_FIELD(lang->muxtweakgen.brightness, "Device Brightness");
    SPECIFIC_FIELD(lang->muxtweakgen.brightness_set, "Setting Device Brightness");
    SPECIFIC_FIELD(lang->muxtweakgen.volume, "Device Volume");
    SPECIFIC_FIELD(lang->muxtweakgen.volume_set, "Setting Device Volume");
    SPECIFIC_FIELD(lang->muxtweakgen.hdmi, "HDMI Output");
    SPECIFIC_FIELD(lang->muxtweakgen.advanced, "Advanced Settings");
    SPECIFIC_FIELD(lang->muxtweakgen.passcode, "Passcode Settings");
    SPECIFIC_FIELD(lang->muxtweakgen.inputremap, "Input Remap");
    SPECIFIC_FIELD(lang->muxtweakgen.displaytemp, "Display Temperature");
    SPECIFIC_FIELD(lang->muxtweakgen.rgb, "Device RGB Lights");
    SPECIFIC_FIELD(lang->muxtweakgen.hkdpad, "DPAD Swap Hotkey");
    SPECIFIC_FIELD(lang->muxtweakgen.hkshot, "Screenshot Hotkey");
    SPECIFIC_FIELD(lang->muxtweakgen.audiosink, "Audio Output");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.title, "Device Startup");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.menu, "Main Menu");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.explore, "Content Explorer");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.collection, "Content Collection");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.history, "History");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.last, "Last Game");
    SPECIFIC_FIELD(lang->muxtweakgen.startup.resume, "Resume Game");
    SPECIFIC_FIELD(lang->muxtweakgen.help.rtc, "Change your current date, time, and timezone");
    SPECIFIC_FIELD(lang->muxtweakgen.help.brightness, "Change the brightness of the device to a specific level");
    SPECIFIC_FIELD(lang->muxtweakgen.help.volume, "Change the volume of the device to a specific level");
    SPECIFIC_FIELD(lang->muxtweakgen.help.hdmi, "Settings to change the HDMI output of the device");
    SPECIFIC_FIELD(
        lang->muxtweakgen.help.advanced, "Settings that should only be changed by those who know what they are doing!"
    );
    SPECIFIC_FIELD(lang->muxtweakgen.help.rgb, "Configure the device fancy RGB light system");
    SPECIFIC_FIELD(lang->muxtweakgen.help.hk_dpad, "Switch between different hotkeys for toggling DPAD swap");
    SPECIFIC_FIELD(lang->muxtweakgen.help.hk_shot, "Switch between different hotkeys for taking a screenshot");
    SPECIFIC_FIELD(lang->muxtweakgen.help.startup, "Change where the device will start up into");
    SPECIFIC_FIELD(lang->muxtweakgen.help.audio_sink, "Select the active Pipewire audio output sink");
    SPECIFIC_FIELD(lang->muxtweakgen.help.pass_code, "Configure boot, launch, and settings passcodes");
    SPECIFIC_FIELD(lang->muxtweakgen.help.input_remap, "Remap controller buttons and axes for the muOS frontend");
    SPECIFIC_FIELD(
        lang->muxtweakgen.help.display_temp, "Configure sunrise and sunset display colour temperature and schedule"
    );
    SPECIFIC_FIELD(
        lang->muxtweakgen.warn,
        "These settings are intended for advanced users.\n\nChanging them incorrectly may cause unexpected behaviour!"
    );

    // muxvisual
    SPECIFIC_FIELD(lang->muxvisual.sort, "Sorting Priority");
    SPECIFIC_FIELD(lang->muxvisual.title, "INTERFACE OPTIONS");
    SPECIFIC_FIELD(lang->muxvisual.battery, "Battery");
    SPECIFIC_FIELD(lang->muxvisual.network, "Network");
    SPECIFIC_FIELD(lang->muxvisual.bluetooth, "Bluetooth");
    SPECIFIC_FIELD(lang->muxvisual.headertitle, "Header Title");
    SPECIFIC_FIELD(lang->muxvisual.dialoguetransition, "Dialogue Transition");
    SPECIFIC_FIELD(lang->muxvisual.clock, "Clock");
    SPECIFIC_FIELD(lang->muxvisual.dash, "Content Dash Replacement");
    SPECIFIC_FIELD(lang->muxvisual.friendlyfolder, "Friendly Folder Names");
    SPECIFIC_FIELD(lang->muxvisual.thetitleformat, "Display Title Reformatting");
    SPECIFIC_FIELD(lang->muxvisual.titleincluderootdrive, "Title Include Root Drive");
    SPECIFIC_FIELD(lang->muxvisual.folderitemcount, "Folder Item Count");
    SPECIFIC_FIELD(lang->muxvisual.displayemptyfolder, "Empty Folders");
    SPECIFIC_FIELD(lang->muxvisual.menucounterfolder, "Menu Counter Folder");
    SPECIFIC_FIELD(lang->muxvisual.menucounterfile, "Menu Counter File");
    SPECIFIC_FIELD(lang->muxvisual.name.title, "Content Name Scheme");
    SPECIFIC_FIELD(lang->muxvisual.name.full, "Full Name");
    SPECIFIC_FIELD(lang->muxvisual.name.rem_sq, "Remove [ ]");
    SPECIFIC_FIELD(lang->muxvisual.name.rem_pa, "Remove ( )");
    SPECIFIC_FIELD(lang->muxvisual.name.rem_sqpa, "Remove [ ] and ( )");
    SPECIFIC_FIELD(lang->muxvisual.hidden, "Show Hidden Content");
    SPECIFIC_FIELD(lang->muxvisual.contentcollect, "Collection In Content");
    SPECIFIC_FIELD(lang->muxvisual.contenthistory, "History In Content");
    SPECIFIC_FIELD(lang->muxvisual.mixedcontent, "Mixed Folder Content");
    SPECIFIC_FIELD(lang->muxvisual.forwardhistory, "Forward History");
    SPECIFIC_FIELD(lang->muxvisual.namescroll, "Label Scroll Style");
    SPECIFIC_FIELD(lang->muxvisual.labelscrollspeed, "Label Scroll Speed");
    SPECIFIC_FIELD(lang->muxvisual.listglyph, "List Glyph");
    SPECIFIC_FIELD(lang->muxvisual.selectionanimation, "Selection Intensity");
    SPECIFIC_FIELD(lang->muxvisual.selectionstyle, "Selection Direction");
    SPECIFIC_FIELD(lang->muxvisual.rendershadows, "Shadow Rendering");
    SPECIFIC_FIELD(lang->muxvisual.scroll_mode.disabled, "Disabled");
    SPECIFIC_FIELD(lang->muxvisual.scroll_mode.continuous, "Continuous");
    SPECIFIC_FIELD(lang->muxvisual.scroll_mode.bounce, "Bounce");
    SPECIFIC_FIELD(lang->muxvisual.overlay.image, "Frontend Overlay Image");
    SPECIFIC_FIELD(lang->muxvisual.overlay.transparency, "Frontend Overlay Transparency");
    SPECIFIC_FIELD(lang->muxvisual.overlay.theme, "Theme Provided");
    SPECIFIC_FIELD(lang->muxvisual.overlay.checkerboard.t1, "Checkerboard (1px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.checkerboard.t4, "Checkerboard (4px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.diagonal.t1, "Diagonal Lines (1px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.diagonal.t2, "Diagonal Lines (2px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.diagonal.t4, "Diagonal Lines (4px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.lattice.t1, "Dot Lattice (1px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.lattice.t4, "Dot Lattice (4px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.horizontal.t1, "Horizontal Lines (1px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.horizontal.t2, "Horizontal Lines (2px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.horizontal.t4, "Horizontal Lines (4px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.vertical.t1, "Vertical Lines (1px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.vertical.t2, "Vertical Lines (2px)");
    SPECIFIC_FIELD(lang->muxvisual.overlay.vertical.t4, "Vertical Lines (4px)");
    SPECIFIC_FIELD(lang->muxvisual.help.sort, "Set sorting priority levels for content");
    SPECIFIC_FIELD(lang->muxvisual.help.battery, "Toggle the visibility of the battery glyph");
    SPECIFIC_FIELD(lang->muxvisual.help.network, "Toggle the visibility of the network glyph");
    SPECIFIC_FIELD(lang->muxvisual.help.bluetooth, "Toggle the visibility of the bluetooth glyph");
    SPECIFIC_FIELD(lang->muxvisual.help.header_title, "Toggle the visibility of the header title");
    SPECIFIC_FIELD(lang->muxvisual.help.dialogue_transition, "Select the animation used when dialogue boxes appear");
    SPECIFIC_FIELD(lang->muxvisual.help.clock, "Toggle the visibility of the clock");
    SPECIFIC_FIELD(lang->muxvisual.help.overlay_image, "Switch between different overlay styles for the frontend only");
    SPECIFIC_FIELD(
        lang->muxvisual.help.overlay_transparency, "Changes the transparency of the overlay image for the frontend only"
    );
    SPECIFIC_FIELD(lang->muxvisual.help.dash, "Replaces the dash (-) with a colon (:) for content labels");
    SPECIFIC_FIELD(
        lang->muxvisual.help.friendly_folder,
        "Replaces the label of shortened content folders to more appropriately named labels"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.the_title_format,
        "Rearranges the label of content to move the 'The' label to the front - For example, 'Batman and "
        "Robin, The' to 'The Batman and Robin'"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.title_include_root_drive,
        "Changes the top title label in Explore Content to show current storage device along with folder name"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.folder_item_count,
        "Toggle the visibility of the item count within folders in Explore Content"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.display_empty_folder, "Toggle the visibility of empty folders in Explore Content"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.menu_counter_folder,
        "Toggle the visibility of currently selected folder along with total in Explore Content"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.menu_counter_file,
        "Toggle the visibility of currently selected file along with total in Explore Content"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.name, "Remove extra information from content labels - This does NOT rename "
                                   "your files it only changes how it is displayed"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.hidden, "Toggle hidden content displayed in Explore Content - Place a '.' or "
                                     "'_' character at the start of a file or folder name to hide it"
    );
    SPECIFIC_FIELD(lang->muxvisual.help.content_collect, "Toggle the collection visibility withing Explore Content");
    SPECIFIC_FIELD(lang->muxvisual.help.content_history, "Toggle the history visibility within Explore Content");
    SPECIFIC_FIELD(
        lang->muxvisual.help.mixed_content,
        "If enabled folders within content explorer will be mixed in with other content alphabetically"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.forward_history,
        "Toggle remembering last selected item when returning to folders in Explore Content"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.name_scroll,
        "Controls how labels scroll when they exceed half the display width - Disabled stops all scrolling, "
        "Continuous loops indefinitely, Bounce scrolls to the end then returns"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.label_scroll_speed,
        "Controls the scroll speed for labels - Disabled turns off all scrolling, Slow and Fast adjust the "
        "animation speed with proportional pause durations"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.list_glyph, "Toggle icon glyphs for menu and content list items - does not "
                                         "affect header, footer, grid, or carousel glyphs"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.selection_animation,
        "Controls the intensity of the selection animation when navigating between list and grid items"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.selection_style,
        "Controls the style of the selection animation applied when navigating between list and grid items"
    );
    SPECIFIC_FIELD(
        lang->muxvisual.help.render_shadows, "Toggle a subtle drop shadow rendered behind list text and glyphs"
    );

    // muxwebserv
    SPECIFIC_FIELD(lang->muxwebserv.title, "WEB SERVICES");
    SPECIFIC_FIELD(lang->muxwebserv.ttyd, "Virtual Terminal");
    SPECIFIC_FIELD(lang->muxwebserv.syncthing, "Syncthing");
    SPECIFIC_FIELD(lang->muxwebserv.sshd, "Secure Shell");
    SPECIFIC_FIELD(lang->muxwebserv.sftpgo, "SFTP + Filebrowser");
    SPECIFIC_FIELD(lang->muxwebserv.tailscaled, "Tailscale");
    SPECIFIC_FIELD(lang->muxwebserv.help.ttyd, "Toggle virtual terminal - WebUI can be found on port 8080");
    SPECIFIC_FIELD(lang->muxwebserv.help.syncthing, "Toggle Syncthing - WebUI can be found on port 7070");
    SPECIFIC_FIELD(lang->muxwebserv.help.sshd, "Toggle SSH support - Access via port 22");
    SPECIFIC_FIELD(lang->muxwebserv.help.sftp_go, "Toggle SFTP support - WebUI can be found on port 9090");
    SPECIFIC_FIELD(lang->muxwebserv.help.tailscaled, "Toggle Tailscale - Need to login via SSH first to configure it");

#undef SYSTEM_FIELD
#undef GENERIC_FIELD
#undef SPECIFIC_FIELD
}
