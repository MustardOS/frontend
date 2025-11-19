#include <string.h>
#include "common.h"
#include "options.h"
#include "language.h"

void load_lang(struct mux_lang *lang) {
    char buffer[MAX_BUFFER_SIZE];
    load_language_file(mux_module);

#define SYSTEM_FIELD(field, string)                     \
        snprintf(buffer, sizeof(buffer), "%s", string), \
        strncpy(field, buffer, MAX_BUFFER_SIZE - 1),    \
        field[MAX_BUFFER_SIZE - 1] = '\0'

#define GENERIC_FIELD(field, string)                                    \
        snprintf(buffer, sizeof(buffer), "%s", string),                 \
        strncpy(field, translate_generic(buffer), MAX_BUFFER_SIZE - 1), \
        field[MAX_BUFFER_SIZE - 1] = '\0'

#define SPECIFIC_FIELD(field, string)                                    \
        snprintf(buffer, sizeof(buffer), "%s", string),                  \
        strncpy(field, translate_specific(buffer), MAX_BUFFER_SIZE - 1), \
        field[MAX_BUFFER_SIZE - 1] = '\0'

    // system language
    SYSTEM_FIELD(lang->SYSTEM.NO_JOY_GENERAL, "Failed to open GENERAL joystick device");
    SYSTEM_FIELD(lang->SYSTEM.NO_JOY_POWER, "Failed to open POWER joystick device");
    SYSTEM_FIELD(lang->SYSTEM.NO_JOY_VOLUME, "Failed to open VOLUME joystick device");
    SYSTEM_FIELD(lang->SYSTEM.NO_JOY_EXTRA, "Failed to open EXTRA joystick device");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_ALLOCATE_MEM, "Failed to allocate memory");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_DUP_STRING, "Failed to duplicate string");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_DIR_OPEN, "Failed to open directory");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_FILE_OPEN, "Failed to open file");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_FILE_WRITE, "Failed to open file for writing");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_FILE_READ, "Failed to open file for reading");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_FORK, "Failed to fork");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_RUN_COMMAND, "Failed to run command");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_READ_COMMAND, "Failed read command output");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_CLOSE_COMMAND, "Failed to close command stream");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_DELETE_FILE, "Failed to delete file");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_CREATE_FILE, "Failed to create file");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_STAT, "Failed to retrieve file or directory status");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_PROC_PART, "Failed to open /proc/partitions");
    SYSTEM_FIELD(lang->SYSTEM.FAIL_INT16_LENGTH, "Failed to use int16 - out of range");

    // generic common language
    GENERIC_FIELD(lang->GENERIC.ADD, "Add");
    GENERIC_FIELD(lang->GENERIC.ADD_COLLECT, "Added to Collection");
    GENERIC_FIELD(lang->GENERIC.ALLOWED, "Allowed");
    GENERIC_FIELD(lang->GENERIC.BACK, "Back");
    GENERIC_FIELD(lang->GENERIC.CHANGE, "Change");
    GENERIC_FIELD(lang->GENERIC.CHANNEL, "Channel");
    GENERIC_FIELD(lang->GENERIC.CHECK, "Check");
    GENERIC_FIELD(lang->GENERIC.CLEAR, "Clear");
    GENERIC_FIELD(lang->GENERIC.CLOSE, "Close");
    GENERIC_FIELD(lang->GENERIC.COLLECT, "Collect");
    GENERIC_FIELD(lang->GENERIC.DIRECTORY, "Directory");
    GENERIC_FIELD(lang->GENERIC.DISABLED, "Disabled");
    GENERIC_FIELD(lang->GENERIC.DOWNLOAD, "Download");
    GENERIC_FIELD(lang->GENERIC.EDIT, "Edit");
    GENERIC_FIELD(lang->GENERIC.ENABLED, "Enabled");
    GENERIC_FIELD(lang->GENERIC.EXCLUDED, "Excluded");
    GENERIC_FIELD(lang->GENERIC.EXTRACT, "Extract");
    GENERIC_FIELD(lang->GENERIC.FILTER, "Filter");
    GENERIC_FIELD(lang->GENERIC.HIDDEN, "Hidden");
    GENERIC_FIELD(lang->GENERIC.HOLD_CONFIRM, "Hold L2 and press X to confirm deletion!");
    GENERIC_FIELD(lang->GENERIC.INCLUDED, "Included");
    GENERIC_FIELD(lang->GENERIC.INDIVIDUAL, "Individual");
    GENERIC_FIELD(lang->GENERIC.INFO, "Info");
    GENERIC_FIELD(lang->GENERIC.INSTALL, "Install");
    GENERIC_FIELD(lang->GENERIC.KIOSK_DISABLE, "This is disabled in kiosk mode!");
    GENERIC_FIELD(lang->GENERIC.LAUNCH, "Launch");
    GENERIC_FIELD(lang->GENERIC.LOAD, "Load");
    GENERIC_FIELD(lang->GENERIC.LOADING, "Loading…");
    GENERIC_FIELD(lang->GENERIC.MIGRATE, "Migrate to SD2");
    GENERIC_FIELD(lang->GENERIC.NEED_CONNECT, "Network connection required…");
    GENERIC_FIELD(lang->GENERIC.NEW, "New");
    GENERIC_FIELD(lang->GENERIC.NOGLYPH, "No Glyph");
    GENERIC_FIELD(lang->GENERIC.NOT_CONNECTED, "Not Connected");
    GENERIC_FIELD(lang->GENERIC.NO_HELP, "No Help Information Found");
    GENERIC_FIELD(lang->GENERIC.NO_INFO, "No Information Found");
    GENERIC_FIELD(lang->GENERIC.OFFLINE, "Offline");
    GENERIC_FIELD(lang->GENERIC.ONLINE, "Online");
    GENERIC_FIELD(lang->GENERIC.OPEN, "Open");
    GENERIC_FIELD(lang->GENERIC.PREVIOUS, "Previous");
    GENERIC_FIELD(lang->GENERIC.REBOOTING, "Rebooting…");
    GENERIC_FIELD(lang->GENERIC.RECURSIVE, "Recursive");
    GENERIC_FIELD(lang->GENERIC.REFRESH, "Refreshing…");
    GENERIC_FIELD(lang->GENERIC.REMOVE, "Remove");
    GENERIC_FIELD(lang->GENERIC.REMOVE_FAIL, "Failed to remove item…");
    GENERIC_FIELD(lang->GENERIC.RESCAN, "Rescan");
    GENERIC_FIELD(lang->GENERIC.RESTORE, "Restore");
    GENERIC_FIELD(lang->GENERIC.RESTRICTED, "Restricted");
    GENERIC_FIELD(lang->GENERIC.SAVE, "Save");
    GENERIC_FIELD(lang->GENERIC.SAVING, "Saving…");
    GENERIC_FIELD(lang->GENERIC.SCROLL, "Scroll");
    GENERIC_FIELD(lang->GENERIC.SELECT, "Select");
    GENERIC_FIELD(lang->GENERIC.SET, "Set");
    GENERIC_FIELD(lang->GENERIC.SHUTTING_DOWN, "Shutting Down…");
    GENERIC_FIELD(lang->GENERIC.SWITCH_IMAGE, "Preview Image");
    GENERIC_FIELD(lang->GENERIC.SWITCH_INFO, "Information");
    GENERIC_FIELD(lang->GENERIC.SYNC, "Sync to SD1");
    GENERIC_FIELD(lang->GENERIC.TOGGLE_ALL, "Toggle All");
    GENERIC_FIELD(lang->GENERIC.TOP, "Top");
    GENERIC_FIELD(lang->GENERIC.UNKNOWN, "Unknown");
    GENERIC_FIELD(lang->GENERIC.USE, "Use");
    GENERIC_FIELD(lang->GENERIC.USER_DEFINED, "User Defined");
    GENERIC_FIELD(lang->GENERIC.VISIBLE, "Visible");

    // muxapp
    SPECIFIC_FIELD(lang->MUXAPP.TITLE, "APPLICATIONS");
    SPECIFIC_FIELD(lang->MUXAPP.LOAD_APP, "Loading Application");
    SPECIFIC_FIELD(lang->MUXAPP.NO_APP, "No Applications Found");
    SPECIFIC_FIELD(lang->MUXAPP.ARCHIVE, "Archive Manager");
    SPECIFIC_FIELD(lang->MUXAPP.TASK, "Task Toolkit");

    // muxappcon
    SPECIFIC_FIELD(lang->MUXAPPCON.TITLE, "APPLICATION OPTION");
    SPECIFIC_FIELD(lang->MUXAPPCON.NAME, "Name");
    SPECIFIC_FIELD(lang->MUXAPPCON.GOVERNOR, "Governor");
    SPECIFIC_FIELD(lang->MUXAPPCON.CONTROL, "Control Scheme");
    SPECIFIC_FIELD(lang->MUXAPPCON.HELP.GOVERNOR, "Set the CPU governor for the selected application");
    SPECIFIC_FIELD(lang->MUXAPPCON.HELP.CONTROL, "Set the control scheme for the selected application");

    // muxarchive
    SPECIFIC_FIELD(lang->MUXARCHIVE.TITLE, "ARCHIVE MANAGER");
    SPECIFIC_FIELD(lang->MUXARCHIVE.INSTALLED, "INSTALLED");
    SPECIFIC_FIELD(lang->MUXARCHIVE.NONE, "No Archives Found");
    SPECIFIC_FIELD(lang->MUXARCHIVE.HELP, "Archive items can be found and installed here - Save games and states, updates, themes etc");

    // muxassign
    SPECIFIC_FIELD(lang->MUXASSIGN.TITLE, "ASSIGN");
    SPECIFIC_FIELD(lang->MUXASSIGN.DIR, "Assigned to directory");
    SPECIFIC_FIELD(lang->MUXASSIGN.FILE, "Assigned to file");
    SPECIFIC_FIELD(lang->MUXASSIGN.NONE, "No Cores Found…");
    SPECIFIC_FIELD(lang->MUXASSIGN.HELP, "This is where you can assign a core or external emulator to content");
    SPECIFIC_FIELD(lang->MUXASSIGN.CORE_DOWN, "Core Downloader");

    // muxbackup
    SPECIFIC_FIELD(lang->MUXBACKUP.TITLE, "DEVICE BACKUP");
    SPECIFIC_FIELD(lang->MUXBACKUP.APPS, "Applications");
    SPECIFIC_FIELD(lang->MUXBACKUP.BIOS, "System BIOS");
    SPECIFIC_FIELD(lang->MUXBACKUP.CATALOGUE, "Metadata Catalogue");
    SPECIFIC_FIELD(lang->MUXBACKUP.CHEATS, "RetroArch Cheats");
    SPECIFIC_FIELD(lang->MUXBACKUP.COLLECTION, "Content Collection");
    SPECIFIC_FIELD(lang->MUXBACKUP.CONFIG, "RetroArch Configs");
    SPECIFIC_FIELD(lang->MUXBACKUP.HISTORY, "History");
    SPECIFIC_FIELD(lang->MUXBACKUP.INIT, "User Init Scripts");
    SPECIFIC_FIELD(lang->MUXBACKUP.MUSIC, "Background Music");
    SPECIFIC_FIELD(lang->MUXBACKUP.MUSIC, "Background Music");
    SPECIFIC_FIELD(lang->MUXBACKUP.NAME, "Friendly Name Configs");
    SPECIFIC_FIELD(lang->MUXBACKUP.NETWORK, "Network Profiles");
    SPECIFIC_FIELD(lang->MUXBACKUP.OVERLAYS, "RetroArch Overlays");
    SPECIFIC_FIELD(lang->MUXBACKUP.OVERRIDE, "Content Launch Overrides");
    SPECIFIC_FIELD(lang->MUXBACKUP.PACKAGE, "Custom Packages");
    SPECIFIC_FIELD(lang->MUXBACKUP.SAVE, "Save Games + Save States");
    SPECIFIC_FIELD(lang->MUXBACKUP.SCREENSHOT, "Screenshots");
    SPECIFIC_FIELD(lang->MUXBACKUP.SHADERS, "RetroArch Shaders");
    SPECIFIC_FIELD(lang->MUXBACKUP.SYNCTHING, "Syncthing Configs");
    SPECIFIC_FIELD(lang->MUXBACKUP.THEME, "Themes");
    SPECIFIC_FIELD(lang->MUXBACKUP.TRACK, "Activity Tracker");
    SPECIFIC_FIELD(lang->MUXBACKUP.TARGET, "Backup Target");
    SPECIFIC_FIELD(lang->MUXBACKUP.MERGE, "Merge Backups");
    SPECIFIC_FIELD(lang->MUXBACKUP.START, "Start Backup");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.APPS, "Location of installed applications");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.BIOS, "Location of system BIOS files");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.CATALOGUE, "Location of content images and text");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.CHEATS, "Location of the RetroArch cheats");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.COLLECTION, "Location of content collection");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.CONFIG, "Location of RetroArch configurations");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.HISTORY, "Location of history");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.INIT, "Location of User Initialisation scripts");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.MUSIC, "Location of background music");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.NAME, "Location of friendly name configurations");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.NETWORK, "Location of Network Profiles");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.OVERLAYS, "Location of the RetroArch overlays");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.OVERRIDE, "Location of the content launch overrides");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.PACKAGE, "Location of custom packages");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.SAVE, "Location of save states and files");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.SCREENSHOT, "Location of screenshots");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.SHADERS, "Location of RetroArch shaders");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.SYNCTHING, "Location of Syncthing configurations");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.THEME, "Location of themes");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.TRACK, "Location of Game Activity Tracker");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.TARGET, "Toggle the target storage device for the backup");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.MERGE, "Merge all backup targets to a single archive");
    SPECIFIC_FIELD(lang->MUXBACKUP.HELP.START, "Start the backup process for the selected items");

    // muxcharge
    SPECIFIC_FIELD(lang->MUXCHARGE.BOOT, "Booting System - Please Wait…");
    SPECIFIC_FIELD(lang->MUXCHARGE.CAPACITY, "Capacity");
    SPECIFIC_FIELD(lang->MUXCHARGE.START, "Press START button to continue booting…");
    SPECIFIC_FIELD(lang->MUXCHARGE.VOLTAGE, "Voltage");

    // muxcollect
    SPECIFIC_FIELD(lang->MUXCOLLECT.TITLE, "COLLECTION");
    SPECIFIC_FIELD(lang->MUXCOLLECT.NONE, "Nothing Saved Yet…");
    SPECIFIC_FIELD(lang->MUXCOLLECT.ERROR.REMOVE_FILE, "Error removing from Collections");
    SPECIFIC_FIELD(lang->MUXCOLLECT.ERROR.REMOVE_DIR, "Collection folder is not empty");
    SPECIFIC_FIELD(lang->MUXCOLLECT.ERROR.LOAD, "Error loading content file");

    // muxconfig
    SPECIFIC_FIELD(lang->MUXCONFIG.TITLE, "CONFIGURATION");
    SPECIFIC_FIELD(lang->MUXCONFIG.CONNECTIVITY, "Connectivity");
    SPECIFIC_FIELD(lang->MUXCONFIG.CUSTOM, "Customisation");
    SPECIFIC_FIELD(lang->MUXCONFIG.GENERAL, "General Settings");
    SPECIFIC_FIELD(lang->MUXCONFIG.POWER, "Power Settings");
    SPECIFIC_FIELD(lang->MUXCONFIG.VISUAL, "Interface Options");
    SPECIFIC_FIELD(lang->MUXCONFIG.LANGUAGE, "Language");
    SPECIFIC_FIELD(lang->MUXCONFIG.STORAGE, "Storage Options");
    SPECIFIC_FIELD(lang->MUXCONFIG.BACKUP, "Device Backup");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.CONNECTIVITY, "Connect your device via Wi-Fi, enable web services, or enable USB functions");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.CUSTOM, "Customise your MustardOS setup with user created packages");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.GENERAL, "Device specific and MustardOS frontend settings can be found here");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.LANGUAGE, "Select your preferred language\n\nTranslations supported by Weblate");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.STORAGE, "Find out what storage device core settings and configurations are mounted");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.POWER, "Settings to change the power features of the device");
    SPECIFIC_FIELD(lang->MUXCONFIG.HELP.VISUAL, "Settings to change the visual aspects of the frontend");

    // muxconnect
    SPECIFIC_FIELD(lang->MUXCONNECT.TITLE, "CONNECTIVITY");
    SPECIFIC_FIELD(lang->MUXCONNECT.BLUETOOTH, "Bluetooth");
    SPECIFIC_FIELD(lang->MUXCONNECT.USB, "USB Function");
    SPECIFIC_FIELD(lang->MUXCONNECT.WEB, "Web Services");
    SPECIFIC_FIELD(lang->MUXCONNECT.WIFI, "Wi-Fi Network");
    SPECIFIC_FIELD(lang->MUXCONNECT.NETADV, "Network Settings");
    SPECIFIC_FIELD(lang->MUXCONNECT.ADB, "Android Debug Bridge");
    SPECIFIC_FIELD(lang->MUXCONNECT.MTP, "Media Transfer Protocol");
    SPECIFIC_FIELD(lang->MUXCONNECT.HELP.WEB, "Toggle a range of configurable services you can access via an active network");
    SPECIFIC_FIELD(lang->MUXCONNECT.HELP.USB, "Toggle between ADB and MTP USB functionality");
    SPECIFIC_FIELD(lang->MUXCONNECT.HELP.WIFI, "Connect to a Wi-Fi network manually or via a saved profile");
    SPECIFIC_FIELD(lang->MUXCONNECT.HELP.NETADV, "Adjust network connectivity settings");
    SPECIFIC_FIELD(lang->MUXCONNECT.HELP.BLUETOOTH, "Toggle the visibility of the bluetooth glyph");

    // muxcontrol
    SPECIFIC_FIELD(lang->MUXCONTROL.TITLE, "CONTROL");
    SPECIFIC_FIELD(lang->MUXCONTROL.HELP, "Change the control scheme of your current selected content");
    SPECIFIC_FIELD(lang->MUXCONTROL.NONE, "No Control Schemes Found…");

    // muxcustom
    SPECIFIC_FIELD(lang->MUXCUSTOM.TITLE, "CUSTOMISATION");
    SPECIFIC_FIELD(lang->MUXCUSTOM.CATALOGUE, "Catalogue Sets");
    SPECIFIC_FIELD(lang->MUXCUSTOM.CONFIG, "RetroArch Configurations");
    SPECIFIC_FIELD(lang->MUXCUSTOM.GRID_MODE_CONTENT, "Content Grid Mode");
    SPECIFIC_FIELD(lang->MUXCUSTOM.THEME_DOWN, "Theme Download");
    SPECIFIC_FIELD(lang->MUXCUSTOM.THEME, "Theme Picker");
    SPECIFIC_FIELD(lang->MUXCUSTOM.THEME_RES, "Theme Resolution");
    SPECIFIC_FIELD(lang->MUXCUSTOM.SCREEN, "Screen");
    SPECIFIC_FIELD(lang->MUXCUSTOM.THEME_ALT, "Alternative Theme");
    SPECIFIC_FIELD(lang->MUXCUSTOM.SPLASH, "Content Launch Splash");
    SPECIFIC_FIELD(lang->MUXCUSTOM.FADE, "Black Fade Animation");
    SPECIFIC_FIELD(lang->MUXCUSTOM.ANIMATION, "Background Animation");
    SPECIFIC_FIELD(lang->MUXCUSTOM.SHUFFLE, "Content Shuffle");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.TITLE, "Content Box Art");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.BEHIND, "Behind");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.FRONT, "Front");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.FS_BEHIND, "Fullscreen + Behind");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.FS_FRONT, "Fullscreen + Front");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.TITLE, "Content Box Art Alignment");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.B_LEFT, "Bottom Left");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.B_MID, "Bottom Middle");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.B_RIGHT, "Bottom Right");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.M_LEFT, "Middle Left");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.M_MID, "Center");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.M_RIGHT, "Middle Right");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.T_LEFT, "Top Left");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.T_MID, "Top Middle");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.ALIGN.T_RIGHT, "Top Right");
    SPECIFIC_FIELD(lang->MUXCUSTOM.BOX_ART.HIDE_GRID_MODE, "Hide Grid Mode Box Art");
    SPECIFIC_FIELD(lang->MUXCUSTOM.FONT.TITLE, "Interface Font Type");
    SPECIFIC_FIELD(lang->MUXCUSTOM.FONT.LANG, "Language");
    SPECIFIC_FIELD(lang->MUXCUSTOM.FONT.THEME, "Theme");
    SPECIFIC_FIELD(lang->MUXCUSTOM.MUSIC.TITLE, "Background Music");
    SPECIFIC_FIELD(lang->MUXCUSTOM.MUSIC.VOLUME, "Background Music Volume");
    SPECIFIC_FIELD(lang->MUXCUSTOM.MUSIC.SET, "Setting Volume");
    SPECIFIC_FIELD(lang->MUXCUSTOM.MUSIC.GLOBAL, "Global");
    SPECIFIC_FIELD(lang->MUXCUSTOM.MUSIC.THEME, "Theme");
    SPECIFIC_FIELD(lang->MUXCUSTOM.SOUND.TITLE, "Navigation Sound");
    SPECIFIC_FIELD(lang->MUXCUSTOM.SOUND.GLOBAL, "Global");
    SPECIFIC_FIELD(lang->MUXCUSTOM.SOUND.THEME, "Theme");
    SPECIFIC_FIELD(lang->MUXCUSTOM.LAUNCH_SWAP.TITLE, "Content Save State Launch");
    SPECIFIC_FIELD(lang->MUXCUSTOM.LAUNCH_SWAP.PRESS_A, "Press A");
    SPECIFIC_FIELD(lang->MUXCUSTOM.LAUNCH_SWAP.HOLD_A, "Hold A");
    SPECIFIC_FIELD(lang->MUXCUSTOM.LAUNCH_SWAP.LOAD_STATE, "Load State");
    SPECIFIC_FIELD(lang->MUXCUSTOM.LAUNCH_SWAP.START_FRESH, "Start Fresh");
    SPECIFIC_FIELD(lang->MUXCUSTOM.CHIME, "Startup Chime");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.CATALOGUE, "Load user created artwork catalogue for content");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.CONFIG, "Load user created RetroArch configurations");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.GRID_MODE_CONTENT, "Allow grid mode for content");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.THEME_DOWN, "Download themes from the MustardOS website");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.THEME, "Change the appearance of the MustardOS frontend launcher");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.THEME_RES, "Allows for testing different theme resolutions");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.THEME_ALT, "Switch between different theme alternatives");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.SPLASH, "Toggle the splash image on content launching");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.FADE, "Toggle the fade to black animation on content launching");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.ANIMATION, "Toggle the background animation of the current selected theme");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.BOX_ART, "Change the display priority of the content images");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.BOX_ALIGN, "Change the screen alignment of the content images");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.BOX_HIDE, "Hide system box art images in Content Explorer when theme uses grid mode");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.FONT, "Change how the font type works in the frontend - 'Theme' will ensure frontend will use fonts within themes with a fallback to language fonts - 'Language' will specifically use language based font");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.MUSIC, "Toggle the background music of the frontend");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.MUSIC_VOLUME, "The volume of the background music currently playing, press A to set current value");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.SOUND, "Toggle the navigation sound of the frontend");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.CHIME, "Toggle the startup chime of the frontend");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.SHUFFLE, "Toggles the ability to shuffle content using the R2 button");
    SPECIFIC_FIELD(lang->MUXCUSTOM.HELP.LAUNCH_SWAP, "Switch between pressing A or holding A to launch content save state automatically");

    // muxdanger
    SPECIFIC_FIELD(lang->MUXDANGER.TITLE, "DANGER SETTINGS");
    SPECIFIC_FIELD(lang->MUXDANGER.VMSWAP, "Swap Tendency");
    SPECIFIC_FIELD(lang->MUXDANGER.DIRTYRATIO, "Write Back Threshold");
    SPECIFIC_FIELD(lang->MUXDANGER.DIRTYBACK, "Background Write Back");
    SPECIFIC_FIELD(lang->MUXDANGER.CACHE, "Cache Reclamation");
    SPECIFIC_FIELD(lang->MUXDANGER.NOMERGE, "I/O Merge Policy");
    SPECIFIC_FIELD(lang->MUXDANGER.REQUESTS, "Queue Depth");
    SPECIFIC_FIELD(lang->MUXDANGER.READAHEAD, "Read Ahead Size");
    SPECIFIC_FIELD(lang->MUXDANGER.PAGECLUSTER, "Swap Read-Ahead");
    SPECIFIC_FIELD(lang->MUXDANGER.TIMESLICE, "Realtime Timeslice");
    SPECIFIC_FIELD(lang->MUXDANGER.IOSTATS, "I/O Stats");
    SPECIFIC_FIELD(lang->MUXDANGER.IDLEFLUSH, "Idle Flush Mode");
    SPECIFIC_FIELD(lang->MUXDANGER.CHILDFIRST, "Fork Optimisation");
    SPECIFIC_FIELD(lang->MUXDANGER.TUNESCALE, "Scheduler Scaling");
    SPECIFIC_FIELD(lang->MUXDANGER.CARDMODE, "Disk Tuning");
    SPECIFIC_FIELD(lang->MUXDANGER.STATE, "Suspend Power State");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.VMSWAP, "Controls how aggressively the system swaps memory to disk\n\nLower values keep processes in RAM longer");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.DIRTYRATIO, "Percentage of RAM allowed to hold unwritten (dirty) data before forcing a write to disk");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.DIRTYBACK, "Background write back starts when dirty data exceeds this percentage of RAM\n\nLower values help reduce latency");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.CACHE, "Higher values reclaim cached file data more aggressively\n\nLower values improve performance for repeated file access");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.NOMERGE, "Controls how kernel merges I/O requests\n\nDisabling merges may benefit specific flash storage performance");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.REQUESTS, "Sets the maximum number of queued I/O requests per device\n\nHigher values increase throughput but use more RAM");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.READAHEAD, "Amount of data preloaded in memory before it's requested\n\nLarger values improve sequential read performance");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.PAGECLUSTER, "Number of pages read ahead when swapping\n\nLower values = less latency, higher = more efficient bulk reads");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.TIMESLICE, "How long real-time tasks run before switching\n\nLower values favour responsiveness, higher values favour throughput");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.IOSTATS, "Enables or disables tracking of per-device I/O statistics\n\nDisabling may improve performance slightly");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.IDLEFLUSH, "Flushes dirty data on idle instead of using write back thresholds\n\nUseful for flash storage or reducing idle power");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.CHILDFIRST, "When enabled, a newly forked child process runs before the parent\n\nMay improve load times for some apps");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.TUNESCALE, "Automatically adjusts scheduler behaviour based on CPU count\n\nDisable for consistent tuning on devices");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.CARDMODE, "Switch between different storage tuning options\n\nMay improve performance on certain mSD cards");
    SPECIFIC_FIELD(lang->MUXDANGER.HELP.STATE, "Switch between system sleep suspend states\n\nChanges how the device reacts to sleep mode and wake locks");

    // muxdevice
    SPECIFIC_FIELD(lang->MUXDEVICE.TITLE, "DEVICE SETTINGS");
    SPECIFIC_FIELD(lang->MUXDEVICE.BLUETOOTH, "Bluetooth Integration");
    SPECIFIC_FIELD(lang->MUXDEVICE.RGB, "RGB LED Integration");
    SPECIFIC_FIELD(lang->MUXDEVICE.DEBUGFS, "Kernel Level DebugFS");
    SPECIFIC_FIELD(lang->MUXDEVICE.HDMI, "HDMI Integration");
    SPECIFIC_FIELD(lang->MUXDEVICE.LID, "Lid Switch Integration");
    SPECIFIC_FIELD(lang->MUXDEVICE.NETWORK, "Network Integration");
    SPECIFIC_FIELD(lang->MUXDEVICE.PORTMASTER, "Portmaster Support");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.BLUETOOTH, "Toggles Bluetooth integration on device");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.RGB, "Toggles RGB LED integration on device");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.DEBUGFS, "Toggles kernel level DebugFS mount on startup\n\nBest to leave this enabled unless otherwise advised!");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.HDMI, "Toggles HDMI integration on device");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.LID, "Toggles hall switch (lid) support on device\n\nDo not enable this on non-lid devices!");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.NETWORK, "Toggles Network integration on device");
    SPECIFIC_FIELD(lang->MUXDEVICE.HELP.PORTMASTER, "Toggles internal Portmaster support on device");

    // muxdownload
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.TITLE.CORE, "CORE DOWNLOADER");
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.TITLE.APP, "APP DOWNLOADER");
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.DOWN.ARCHIVE, "Downloading Archive");
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.DOWN.DATA, "Downloading Data");
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.REFRESH, "Refresh");
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.ARCHIVE_REMOVED, "Archive Removed");
    SPECIFIC_FIELD(lang->MUXDOWNLOAD.ERROR_GET_DATA, "Error Retrieving Data");

    // muxgov
    SPECIFIC_FIELD(lang->MUXGOV.TITLE, "GOVERNOR");
    SPECIFIC_FIELD(lang->MUXGOV.HELP, "Configure CPU governors to dynamically adjust the CPU frequency and help balance power consumption and performance");
    SPECIFIC_FIELD(lang->MUXGOV.NONE, "No Governors Found…");

    // muxhdmi
    SPECIFIC_FIELD(lang->MUXHDMI.TITLE, "HDMI SETTINGS");
    SPECIFIC_FIELD(lang->MUXHDMI.RESOLUTION, "Resolution");
    SPECIFIC_FIELD(lang->MUXHDMI.COLOUR.DEPTH, "Colour Depth");
    SPECIFIC_FIELD(lang->MUXHDMI.COLOUR.SPACE, "Colour Space");
    SPECIFIC_FIELD(lang->MUXHDMI.COLOUR.RANGE.TITLE, "Colour Range");
    SPECIFIC_FIELD(lang->MUXHDMI.COLOUR.RANGE.FULL, "Full");
    SPECIFIC_FIELD(lang->MUXHDMI.COLOUR.RANGE.LIMITED, "Limited");
    SPECIFIC_FIELD(lang->MUXHDMI.AUDIO_OUTPUT.TITLE, "Audio Output");
    SPECIFIC_FIELD(lang->MUXHDMI.AUDIO_OUTPUT.EXTERNAL, "External");
    SPECIFIC_FIELD(lang->MUXHDMI.AUDIO_OUTPUT.INTERNAL, "Internal");
    SPECIFIC_FIELD(lang->MUXHDMI.SCAN_SCALE.TITLE, "Scan Scaling");
    SPECIFIC_FIELD(lang->MUXHDMI.SCAN_SCALE.OVER, "Over");
    SPECIFIC_FIELD(lang->MUXHDMI.SCAN_SCALE.UNDER, "Under");
    SPECIFIC_FIELD(lang->MUXHDMI.HELP.AUDIO_OUTPUT, "Switch between device speaker or external monitor audio via HDMI connection");
    SPECIFIC_FIELD(lang->MUXHDMI.HELP.RESOLUTION, "Select the resolution for HDMI output, such as 720p or 1080p");
    SPECIFIC_FIELD(lang->MUXHDMI.HELP.SCAN_SCALE, "Switch between overscan or underscan to fit the display screen");
    SPECIFIC_FIELD(lang->MUXHDMI.HELP.COLOUR.DEPTH, "Set the color depth, such as 8-bit or 10-bit");
    SPECIFIC_FIELD(lang->MUXHDMI.HELP.COLOUR.RANGE, "Set the color range of RGB colour space");
    SPECIFIC_FIELD(lang->MUXHDMI.HELP.COLOUR.SPACE, "Set the color space, such as RGB or YUV");

    // muxhistory
    SPECIFIC_FIELD(lang->MUXHISTORY.TITLE, "HISTORY");
    SPECIFIC_FIELD(lang->MUXHISTORY.NONE, "Nothing Played Yet…");
    SPECIFIC_FIELD(lang->MUXHISTORY.REMOVE, "Removed from History");
    SPECIFIC_FIELD(lang->MUXHISTORY.ERROR.REMOVE, "Error removing from History");
    SPECIFIC_FIELD(lang->MUXHISTORY.ERROR.LOAD, "Error loading content file");

    // muxinfo
    SPECIFIC_FIELD(lang->MUXINFO.TITLE, "INFORMATION");
    SPECIFIC_FIELD(lang->MUXINFO.SYSINFO, "System Details");
    SPECIFIC_FIELD(lang->MUXINFO.NETINFO, "Network Details");
    SPECIFIC_FIELD(lang->MUXINFO.SCREENSHOT, "Screenshots");
    SPECIFIC_FIELD(lang->MUXINFO.SPACE, "Storage Space");
    SPECIFIC_FIELD(lang->MUXINFO.INPUT, "Input Tester");
    SPECIFIC_FIELD(lang->MUXINFO.CREDIT, "Supporters and Credits");
    SPECIFIC_FIELD(lang->MUXINFO.HELP.SYSINFO, "Access version information and system details");
    SPECIFIC_FIELD(lang->MUXINFO.HELP.NETINFO, "Access network information");
    SPECIFIC_FIELD(lang->MUXINFO.HELP.SCREENSHOT, "View all of the screenshots taken on the device");
    SPECIFIC_FIELD(lang->MUXINFO.HELP.SPACE, "View the current used space of the mounted storage devices");
    SPECIFIC_FIELD(lang->MUXINFO.HELP.INPUT, "Test the controls of the device");
    SPECIFIC_FIELD(lang->MUXINFO.HELP.CREDIT, "View all of the current MustardOS supporters and extra credits");

    // muxinstall
    SPECIFIC_FIELD(lang->MUXINSTALL.TITLE, "INSTALLER");
    SPECIFIC_FIELD(lang->MUXINSTALL.DATETIME, "Date and Time");
    SPECIFIC_FIELD(lang->MUXINSTALL.LANGUAGE, "Language");
    SPECIFIC_FIELD(lang->MUXINSTALL.INSTALL, "Install");
    SPECIFIC_FIELD(lang->MUXINSTALL.SHUTDOWN, "Shutdown");
    SPECIFIC_FIELD(lang->MUXINSTALL.SHORT.DATETIME, "Clock");
    SPECIFIC_FIELD(lang->MUXINSTALL.SHORT.LANGUAGE, "Language");
    SPECIFIC_FIELD(lang->MUXINSTALL.SHORT.INSTALL, "Install");
    SPECIFIC_FIELD(lang->MUXINSTALL.SHORT.SHUTDOWN, "Shutdown");
    SPECIFIC_FIELD(lang->MUXINSTALL.HELP.DATETIME, "Change your current date, time, and timezone");
    SPECIFIC_FIELD(lang->MUXINSTALL.HELP.LANGUAGE, "Select your preferred language\n\nTranslations supported by Weblate");
    SPECIFIC_FIELD(lang->MUXINSTALL.HELP.INSTALL, "Prepare and install MustardOS");
    SPECIFIC_FIELD(lang->MUXINSTALL.HELP.SHUTDOWN, "Shut down your device safely");

    // muxkiosk
    SPECIFIC_FIELD(lang->MUXKIOSK.TITLE, "KIOSK SETTINGS");
    SPECIFIC_FIELD(lang->MUXKIOSK.ENABLE, "Kiosk Mode");
    SPECIFIC_FIELD(lang->MUXKIOSK.MESSAGE, "Restricted Messages");
    SPECIFIC_FIELD(lang->MUXKIOSK.ARCHIVE, "Archive Manager");
    SPECIFIC_FIELD(lang->MUXKIOSK.TASK, "Task Toolkit");
    SPECIFIC_FIELD(lang->MUXKIOSK.CUSTOM, "Customisation");
    SPECIFIC_FIELD(lang->MUXKIOSK.LANGUAGE, "Language");
    SPECIFIC_FIELD(lang->MUXKIOSK.NETWORK, "Wi-Fi Network");
    SPECIFIC_FIELD(lang->MUXKIOSK.STORAGE, "Storage");
    SPECIFIC_FIELD(lang->MUXKIOSK.BACKUP, "Backup");
    SPECIFIC_FIELD(lang->MUXKIOSK.NETADV, "Network Settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.WEBSERV, "Web Services");
    SPECIFIC_FIELD(lang->MUXKIOSK.CORE, "Content Core");
    SPECIFIC_FIELD(lang->MUXKIOSK.GOVERNOR, "Content Governor");
    SPECIFIC_FIELD(lang->MUXKIOSK.CONTROL, "Content Control Scheme");
    SPECIFIC_FIELD(lang->MUXKIOSK.OPTION, "Content Options");
    SPECIFIC_FIELD(lang->MUXKIOSK.RETROARCH, "RetroArch Kiosk");
    SPECIFIC_FIELD(lang->MUXKIOSK.SEARCH, "Content Search");
    SPECIFIC_FIELD(lang->MUXKIOSK.TAG, "Content Tag");
    SPECIFIC_FIELD(lang->MUXKIOSK.CATALOGUE, "Custom Catalogue");
    SPECIFIC_FIELD(lang->MUXKIOSK.RACONFIG, "Custom RetroArch Configs");
    SPECIFIC_FIELD(lang->MUXKIOSK.THEME, "Custom Themes");
    SPECIFIC_FIELD(lang->MUXKIOSK.THEME_DOWN, "Theme Download");
    SPECIFIC_FIELD(lang->MUXKIOSK.CLOCK, "Date and Time");
    SPECIFIC_FIELD(lang->MUXKIOSK.TIMEZONE, "Timezone");
    SPECIFIC_FIELD(lang->MUXKIOSK.APPS, "Applications");
    SPECIFIC_FIELD(lang->MUXKIOSK.CONFIG, "Configuration");
    SPECIFIC_FIELD(lang->MUXKIOSK.EXPLORE, "Content Explorer");
    SPECIFIC_FIELD(lang->MUXKIOSK.INFO, "Information");
    SPECIFIC_FIELD(lang->MUXKIOSK.ADVANCED, "Advanced Settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.GENERAL, "General Settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HDMI, "HDMI Settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.POWER, "Power Settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.VISUAL, "Interface Options");
    SPECIFIC_FIELD(lang->MUXKIOSK.COLLECTION.MAIN, "Collection Viewing");
    SPECIFIC_FIELD(lang->MUXKIOSK.COLLECTION.ADD_CONTENT, "Collection Adding Content");
    SPECIFIC_FIELD(lang->MUXKIOSK.COLLECTION.NEW_DIR, "Collection Folder Creation");
    SPECIFIC_FIELD(lang->MUXKIOSK.COLLECTION.REMOVE, "Collection Removal");
    SPECIFIC_FIELD(lang->MUXKIOSK.COLLECTION.ACCESS, "Collection Access");
    SPECIFIC_FIELD(lang->MUXKIOSK.HISTORY.MAIN, "History Viewing");
    SPECIFIC_FIELD(lang->MUXKIOSK.HISTORY.REMOVE, "History Removal");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.ENABLE, "Enable or disable kiosk mode restrictions");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.MESSAGE, "Enable or disable kiosk mode messages");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.ARCHIVE, "Allow access to archive manager");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.TASK, "Permit access to the task toolkit");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.CUSTOM, "Allow users to customise the interface");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.LANGUAGE, "Permit changing the system language");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.NETWORK, "Allow editing of network settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.STORAGE, "Allow user to migrate or sync user based content");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.BACKUP, "Allow the use of the user based content backup tool");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.NETADV, "Enable access to network settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.WEBSERV, "Enable use of web based features");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.CORE, "Allow selection of content cores");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.GOVERNOR, "Allow selection of content governor");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.CONTROL, "Allow selection of content control scheme");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.OPTION, "Allow users to adjust content options");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.RETROARCH, "Enable or restrict RetroArch settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.SEARCH, "Allow searching for content");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.TAG, "Permit tagging and metadata changes");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.CATALOGUE, "Allow access to install content catalogues");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.RACONFIG, "Allow access to use custom RetroArch configs");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.THEME, "Permit changing system themes");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.THEME_DOWN, "Permit downloading of themes");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.CLOCK, "Allow changing the system clock");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.TIMEZONE, "Permit adjusting the timezone");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.APPS, "Enable access to applications");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.CONFIG, "Enable access to system configuration");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.EXPLORE, "Permit access to the content explorer");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.INFO, "Allow viewing system information");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.ADVANCED, "Enable access to advanced settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.GENERAL, "Enable access to general settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.HDMI, "Allow modifying HDMI display settings");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.POWER, "Permit power options such as shutdown or sleep");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.VISUAL, "Allow changes to interface visuals");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.COLLECTION.MAIN, "Enable viewing of content collections");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.COLLECTION.ADD_CONTENT, "Permit adding content to collections");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.COLLECTION.NEW_DIR, "Allow creation of new collection folders");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.COLLECTION.REMOVE, "Permit deletion of collections");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.COLLECTION.ACCESS, "Restrict to specific 'kiosk' directory within collections");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.HISTORY.MAIN, "Enable access to recently used content");
    SPECIFIC_FIELD(lang->MUXKIOSK.HELP.HISTORY.REMOVE, "Allow clearing items from history");

    // muxlanguage
    SPECIFIC_FIELD(lang->MUXLANGUAGE.TITLE, "LANGUAGE");
    SPECIFIC_FIELD(lang->MUXLANGUAGE.NONE, "No Languages Found…");
    SPECIFIC_FIELD(lang->MUXLANGUAGE.SAVE, "Saving Language");
    SPECIFIC_FIELD(lang->MUXLANGUAGE.HELP, "Select your preferred language");
    SPECIFIC_FIELD(lang->MUXLANGUAGE.REFRESH, "Refresh");
    SPECIFIC_FIELD(lang->MUXLANGUAGE.DOWNLOADING, "Downloading Language Updates");
    SPECIFIC_FIELD(lang->MUXLANGUAGE.ERROR_GET_DATA, "Error Retrieving Language Data");

    // muxlaunch
    SPECIFIC_FIELD(lang->MUXLAUNCH.TITLE, "MAIN MENU");
    SPECIFIC_FIELD(lang->MUXLAUNCH.APP, "Applications");
    SPECIFIC_FIELD(lang->MUXLAUNCH.CONFIG, "Configuration");
    SPECIFIC_FIELD(lang->MUXLAUNCH.INFO, "Information");
    SPECIFIC_FIELD(lang->MUXLAUNCH.COLLECTION, "Collection");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HISTORY, "History");
    SPECIFIC_FIELD(lang->MUXLAUNCH.EXPLORE, "Explore Content");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHUTDOWN, "Shutdown");
    SPECIFIC_FIELD(lang->MUXLAUNCH.REBOOT, "Reboot");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.APP, "Apps");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.CONFIG, "Config");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.INFO, "Info");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.COLLECTION, "Collection");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.HISTORY, "History");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.EXPLORE, "Content");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.SHUTDOWN, "Shutdown");
    SPECIFIC_FIELD(lang->MUXLAUNCH.SHORT.REBOOT, "Reboot");
    SPECIFIC_FIELD(lang->MUXLAUNCH.KIOSK.ERROR, "Kiosk configuration not found");
    SPECIFIC_FIELD(lang->MUXLAUNCH.KIOSK.PROCESS, "Processing kiosk configuration");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.APP, "Various applications can be found and launched here");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.CONFIG, "Various configurations can be changed here");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.INFO, "Various information can be found and launched here");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.COLLECTION, "Content added to collections can be found and launched here");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.HISTORY, "Content previously launched can be found and launched here");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.EXPLORE, "Content on storage devices (SD1/SD2/USB) can be found and launched here");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.SHUTDOWN, "Shut down your device safely");
    SPECIFIC_FIELD(lang->MUXLAUNCH.HELP.REBOOT, "Reboot your device safely");

    // muxnetadv
    SPECIFIC_FIELD(lang->MUXNETADV.TITLE, "NETWORK SETTINGS");
    SPECIFIC_FIELD(lang->MUXNETADV.MONITOR, "Connection Monitor");
    SPECIFIC_FIELD(lang->MUXNETADV.BOOT, "Start Network on Boot");
    SPECIFIC_FIELD(lang->MUXNETADV.WAKE, "Start Network on Wake");
    SPECIFIC_FIELD(lang->MUXNETADV.COMPAT, "Module Compatibility");
    SPECIFIC_FIELD(lang->MUXNETADV.ASYNCLOAD, "Module Async Load");
    SPECIFIC_FIELD(lang->MUXNETADV.WAIT, "Module Wait Timer");
    SPECIFIC_FIELD(lang->MUXNETADV.RETRY, "Module Retry");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.MONITOR, "Enables periodic connectivity checks and triggers reconnection if network loss is detected");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.BOOT, "Enables network connection to be established automatically at boot");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.WAKE, "Enables network connection to be re-established automatically upon suspend wake");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.COMPAT, "Enable device compatibility with network module loading via the Linux kernel\n\nIncreases boot times moderately");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.ASYNCLOAD, "Enable the background handling of compatibility handling\n\nProvides faster boot times with compatibility enabled, disable this only if all else has failed");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.WAIT, "Adjusts the maximum amount of time waiting for the network interface to appear.\n\nWARNING:\nIf you enable Module Compatibility, it is not advisable to increase this setting as it will increase boot times!");
    SPECIFIC_FIELD(lang->MUXNETADV.HELP.RETRY, "Adjusts the maximum amount of attempts at loading the network module with Module Compatibility enabled. Increase this setting if you are still unable to connect to wifi.\n\nIncreasing this setting may increase boot times importantly.");

    // muxnetinfo
    SPECIFIC_FIELD(lang->MUXNETINFO.TITLE, "NETWORK DETAILS");
    SPECIFIC_FIELD(lang->MUXNETINFO.HOSTNAME, "Hostname");
    SPECIFIC_FIELD(lang->MUXNETINFO.MAC, "MAC Address");
    SPECIFIC_FIELD(lang->MUXNETINFO.IP, "IP Address");
    SPECIFIC_FIELD(lang->MUXNETINFO.SSID, "Access Point");
    SPECIFIC_FIELD(lang->MUXNETINFO.GATEWAY, "Gateway");
    SPECIFIC_FIELD(lang->MUXNETINFO.DNS, "DNS");
    SPECIFIC_FIELD(lang->MUXNETINFO.SIGNAL, "Signal");
    SPECIFIC_FIELD(lang->MUXNETINFO.CHANNEL, "Channel");
    SPECIFIC_FIELD(lang->MUXNETINFO.ACTRAFFIC, "Accumulated Traffic");
    SPECIFIC_FIELD(lang->MUXNETINFO.TPTRAFFIC, "Throughput Traffic");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.HOSTNAME, "The current hostname of this device");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.MAC, "The unique hardware address of the network interface");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.IP, "The current IP address assigned to this device");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.SSID, "The name (SSID) of the connected Wi-Fi network");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.GATEWAY, "The network gateway used to reach external networks");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.DNS, "The DNS servers used to resolve domain names");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.SIGNAL, "The Wi-Fi signal strength expressed as a percentage");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.CHANNEL, "The Wi-Fi frequency and connected channel of the access point");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.ACTRAFFIC, "The total data sent and received over the network");
    SPECIFIC_FIELD(lang->MUXNETINFO.HELP.TPTRAFFIC, "The current data sent and received over the network");
    SPECIFIC_FIELD(lang->MUXNETINFO.ERROR.EDIT, "Cannot edit if network is active!");
    SPECIFIC_FIELD(lang->MUXNETINFO.ERROR.CHANGE, "Cannot change if network is active!");

    // muxnetprofile
    SPECIFIC_FIELD(lang->MUXNETPROFILE.TITLE, "NETWORK PROFILE");
    SPECIFIC_FIELD(lang->MUXNETPROFILE.LOAD, "Loading Network Profiles…");
    SPECIFIC_FIELD(lang->MUXNETPROFILE.NONE, "No Saved Network Profiles Found");
    SPECIFIC_FIELD(lang->MUXNETPROFILE.HELP, "Quickly switch between different Wi-Fi configurations based on your location or network preferences");
    SPECIFIC_FIELD(lang->MUXNETPROFILE.INVALID_SSID, "Invalid SSID");
    SPECIFIC_FIELD(lang->MUXNETPROFILE.INVALID_NETWORK, "Invalid Network Settings");

    // muxnetscan
    SPECIFIC_FIELD(lang->MUXNETSCAN.TITLE, "NETWORK SCAN");
    SPECIFIC_FIELD(lang->MUXNETSCAN.SCAN, "Scanning for Wi-Fi Networks…");
    SPECIFIC_FIELD(lang->MUXNETSCAN.NONE, "No Wi-Fi Networks Found");
    SPECIFIC_FIELD(lang->MUXNETSCAN.HELP, "Detect, display and connect to available Wi-Fi networks");

    // muxnetwork
    SPECIFIC_FIELD(lang->MUXNETWORK.TITLE, "WI-FI NETWORK");
    SPECIFIC_FIELD(lang->MUXNETWORK.CONNECT, "Connect");
    SPECIFIC_FIELD(lang->MUXNETWORK.DISCONNECT, "Disconnect");
    SPECIFIC_FIELD(lang->MUXNETWORK.CONNECTED, "Connected");
    SPECIFIC_FIELD(lang->MUXNETWORK.NOT_CONNECTED, "Not Connected");
    SPECIFIC_FIELD(lang->MUXNETWORK.DENY_MODIFY, "Cannot modify while connected");
    SPECIFIC_FIELD(lang->MUXNETWORK.SAVE, "Changes Saved");
    SPECIFIC_FIELD(lang->MUXNETWORK.DHCP, "DHCP");
    SPECIFIC_FIELD(lang->MUXNETWORK.STATIC, "Static");
    SPECIFIC_FIELD(lang->MUXNETWORK.SCAN, "Scan");
    SPECIFIC_FIELD(lang->MUXNETWORK.CIDR, "Subnet CIDR");
    SPECIFIC_FIELD(lang->MUXNETWORK.PROFILES, "Profiles");
    SPECIFIC_FIELD(lang->MUXNETWORK.CONNECT_TRY, "Trying to Connect…");
    SPECIFIC_FIELD(lang->MUXNETWORK.PASSWORD, "Password");
    SPECIFIC_FIELD(lang->MUXNETWORK.NO_PASSWORD, "No Password Detected…");
    SPECIFIC_FIELD(lang->MUXNETWORK.DNS, "DNS Server");
    SPECIFIC_FIELD(lang->MUXNETWORK.IP, "Device IP");
    SPECIFIC_FIELD(lang->MUXNETWORK.ENCRYPT_PASSWORD, "Encrypting Password…");
    SPECIFIC_FIELD(lang->MUXNETWORK.GATEWAY, "Gateway IP");
    SPECIFIC_FIELD(lang->MUXNETWORK.HIDDEN, "Hidden Network");
    SPECIFIC_FIELD(lang->MUXNETWORK.SSID, "Identifier");
    SPECIFIC_FIELD(lang->MUXNETWORK.DISABLED, "Network Disabled");
    SPECIFIC_FIELD(lang->MUXNETWORK.TYPE, "Network Type");
    SPECIFIC_FIELD(lang->MUXNETWORK.CHECK, "Please check network settings");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.TYPE, "Toggle between DHCP and Static network types");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.HIDDEN, "Toggle whether or not to try and connect to a hidden SSID broadcast");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.PASSWORD, "Enter the network password here (optional)");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.SSID, "Enter the network identifier (SSID) here");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.GATEWAY, "Enter the network gateway address here (Static only)");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.CIDR, "Enter the device Subnet (CIDR) number here (Static only)");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.IP, "Enter the device IP address here (Static only)");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.DNS, "Enter the device DNS address here (Static only)");
    SPECIFIC_FIELD(lang->MUXNETWORK.HELP.CONNECT, "Connect to the network using options entered above");

    // muxoption
    SPECIFIC_FIELD(lang->MUXOPTION.TITLE, "CONTENT OPTION");
    SPECIFIC_FIELD(lang->MUXOPTION.SEARCH, "Search");
    SPECIFIC_FIELD(lang->MUXOPTION.NAME, "Name");
    SPECIFIC_FIELD(lang->MUXOPTION.TIME, "Time Played");
    SPECIFIC_FIELD(lang->MUXOPTION.LAUNCH, "Times Launched");
    SPECIFIC_FIELD(lang->MUXOPTION.CURRENT, "Current");
    SPECIFIC_FIELD(lang->MUXOPTION.DIRECTORY, "Directory");
    SPECIFIC_FIELD(lang->MUXOPTION.INDIVIDUAL, "Individual");
    SPECIFIC_FIELD(lang->MUXOPTION.CORE, "Core");
    SPECIFIC_FIELD(lang->MUXOPTION.GOVERNOR, "Governor");
    SPECIFIC_FIELD(lang->MUXOPTION.CONTROL, "Control Scheme");
    SPECIFIC_FIELD(lang->MUXOPTION.TAG, "Glyph Tag");
    SPECIFIC_FIELD(lang->MUXOPTION.NONE, "None");
    SPECIFIC_FIELD(lang->MUXOPTION.NOT_ASSIGNED, "Not Assigned");
    SPECIFIC_FIELD(lang->MUXOPTION.HELP.CORE, "Set the system core or external emulator for the selected content or directory");
    SPECIFIC_FIELD(lang->MUXOPTION.HELP.GOVERNOR, "Set the CPU governor for the selected content or directory");
    SPECIFIC_FIELD(lang->MUXOPTION.HELP.CONTROL, "Set the control scheme for the selected content or directory");
    SPECIFIC_FIELD(lang->MUXOPTION.HELP.TAG, "Set the specific tag of the content selected to change the glyph");
    SPECIFIC_FIELD(lang->MUXOPTION.HELP.SEARCH, "Search for content within the selected directory");

    // muxpass
    SPECIFIC_FIELD(lang->MUXPASS.TITLE, "PASSCODE");

    // muxpicker
    SPECIFIC_FIELD(lang->MUXPICKER.CUSTOM, "CUSTOM PICKER");
    SPECIFIC_FIELD(lang->MUXPICKER.CATALOGUE, "CATALOGUE PICKER");
    SPECIFIC_FIELD(lang->MUXPICKER.CONFIG, "CONFIG PICKER");
    SPECIFIC_FIELD(lang->MUXPICKER.THEME, "THEME PICKER");
    SPECIFIC_FIELD(lang->MUXPICKER.THEME_DOWN, "Theme Download");
    SPECIFIC_FIELD(lang->MUXPICKER.INVALID_VER, "Incompatible Theme Version Detected");
    SPECIFIC_FIELD(lang->MUXPICKER.INVALID_RES, "Incompatible Theme Resolution Detected");
    SPECIFIC_FIELD(lang->MUXPICKER.PROTECTED, "This theme is protected from deletion!");
    SPECIFIC_FIELD(lang->MUXPICKER.NONE.CREDIT, "There are no attributed credits!");
    SPECIFIC_FIELD(lang->MUXPICKER.NONE.CUSTOM, "No Custom Packages Found");
    SPECIFIC_FIELD(lang->MUXPICKER.NONE.CATALOGUE, "No Catalogue Packages Found");
    SPECIFIC_FIELD(lang->MUXPICKER.NONE.CONFIG, "No Configuration Packages Found");
    SPECIFIC_FIELD(lang->MUXPICKER.NONE.THEME, "No Theme Packages Found");

    // muxplore
    SPECIFIC_FIELD(lang->MUXPLORE.TITLE, "EXPLORE");
    SPECIFIC_FIELD(lang->MUXPLORE.REFRESH, "Refresh");
    SPECIFIC_FIELD(lang->MUXPLORE.REFRESH_RUN, "Refreshing…");
    SPECIFIC_FIELD(lang->MUXPLORE.NONE, "No Content Found…");
    SPECIFIC_FIELD(lang->MUXPLORE.ERROR.NO_FOLDER, "Folders cannot be added to collections");
    SPECIFIC_FIELD(lang->MUXPLORE.ERROR.NO_CORE, "Content is not associated with system or core");
    SPECIFIC_FIELD(lang->MUXPLORE.ERROR.GENERAL, "Could not load content");

    // muxpower
    SPECIFIC_FIELD(lang->MUXPOWER.TITLE, "POWER SETTINGS");
    SPECIFIC_FIELD(lang->MUXPOWER.LOW_BATTERY, "Low Battery Indicator");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.ERROR, "Idle Display must be less than Idle Sleep");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.DISPLAY, "Idle Input Display Timeout");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.SLEEP, "Idle Input Sleep Timeout");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.MUTE, "Mute on Display Timeout");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t10s, "10s");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t30s, "30s");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t60s, "60s");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t2m, "2m");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t5m, "5m");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t10m, "10m");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t15m, "15m");
    SPECIFIC_FIELD(lang->MUXPOWER.IDLE.t30m, "30m");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.TITLE, "Sleep Function");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.INSTANT, "Instant Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.SUSPEND, "Sleep Until Wake");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t10s, "Sleep 10s + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t30s, "Sleep 30s + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t60s, "Sleep 60s + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t2m, "Sleep 2m + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t5m, "Sleep 5m + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t10m, "Sleep 10m + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t15m, "Sleep 15m + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t30m, "Sleep 30m + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.SLEEP.t60m, "Sleep 60m + Shutdown");
    SPECIFIC_FIELD(lang->MUXPOWER.GOV.IDLE, "Idle Governor");
    SPECIFIC_FIELD(lang->MUXPOWER.GOV.DEFAULT, "Default Governor");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.IDLE.SLEEP, "Configure the time the device will sleep when no input is detected");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.IDLE.DISPLAY, "Configure the time the screen will dim when no input is detected");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.IDLE.MUTE, "Toggle if the audio is muted when display is dimmed");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.GOV.IDLE, "Configure the frontend and device power governor on idle input");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.GOV.DEFAULT, "Configure the default frontend and device power governor");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.LOW_BATTERY, "Configure when the red LED will display based on the current capacity percentage");
    SPECIFIC_FIELD(lang->MUXPOWER.HELP.SLEEP_FUNCTION, "Configure how the power button functions on short press");

    // muxrtc
    SPECIFIC_FIELD(lang->MUXRTC.TITLE, "DATE AND TIME");
    SPECIFIC_FIELD(lang->MUXRTC.DAY, "Day");
    SPECIFIC_FIELD(lang->MUXRTC.MONTH, "Month");
    SPECIFIC_FIELD(lang->MUXRTC.YEAR, "Year");
    SPECIFIC_FIELD(lang->MUXRTC.HOUR, "Hour");
    SPECIFIC_FIELD(lang->MUXRTC.MINUTE, "Minute");
    SPECIFIC_FIELD(lang->MUXRTC.TIMEZONE, "Set Timezone");
    SPECIFIC_FIELD(lang->MUXRTC.NOTATION, "Time Notation");
    SPECIFIC_FIELD(lang->MUXRTC.F_12HR, "12 Hour");
    SPECIFIC_FIELD(lang->MUXRTC.F_24HR, "24 Hour");
    SPECIFIC_FIELD(lang->MUXRTC.HELP, "Change your current date, time, and timezone");

    // muxsearch
    SPECIFIC_FIELD(lang->MUXSEARCH.TITLE, "SEARCH CONTENT");
    SPECIFIC_FIELD(lang->MUXSEARCH.GLOBAL, "Search Global");
    SPECIFIC_FIELD(lang->MUXSEARCH.LOCAL, "Search Local");
    SPECIFIC_FIELD(lang->MUXSEARCH.LOOKUP, "Lookup");
    SPECIFIC_FIELD(lang->MUXSEARCH.SEARCH, "Searching…");
    SPECIFIC_FIELD(lang->MUXSEARCH.ERROR, "Lookup has to be 3 characters or more!");
    SPECIFIC_FIELD(lang->MUXSEARCH.HELP.GLOBAL, "Search all current active storage devices");
    SPECIFIC_FIELD(lang->MUXSEARCH.HELP.LOCAL, "Search within the current selected folder and folders within");
    SPECIFIC_FIELD(lang->MUXSEARCH.HELP.LOOKUP, "Enter in the name of the content you are looking for");

    // muxshot
    SPECIFIC_FIELD(lang->MUXSHOT.TITLE, "SCREENSHOTS");
    SPECIFIC_FIELD(lang->MUXSHOT.HELP, "View your current screenshots");
    SPECIFIC_FIELD(lang->MUXSHOT.NONE, "No Screenshots Found");

    // muxspace
    SPECIFIC_FIELD(lang->MUXSPACE.TITLE, "STORAGE SPACE");
    SPECIFIC_FIELD(lang->MUXSPACE.HELP, "View the current used space of the mounted storage devices");
    SPECIFIC_FIELD(lang->MUXSPACE.PRIMARY, "Primary Storage");
    SPECIFIC_FIELD(lang->MUXSPACE.SECONDARY, "Secondary Storage");
    SPECIFIC_FIELD(lang->MUXSPACE.EXTERNAL, "External Storage");
    SPECIFIC_FIELD(lang->MUXSPACE.SYSTEM, "System Storage");

    // muxstorage
    SPECIFIC_FIELD(lang->MUXSTORAGE.TITLE, "STORAGE");
    SPECIFIC_FIELD(lang->MUXSTORAGE.APPS, "Applications");
    SPECIFIC_FIELD(lang->MUXSTORAGE.BIOS, "System BIOS");
    SPECIFIC_FIELD(lang->MUXSTORAGE.CATALOGUE, "Metadata Catalogue");
    SPECIFIC_FIELD(lang->MUXSTORAGE.COLLECTION, "Content Collection");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HISTORY, "History");
    SPECIFIC_FIELD(lang->MUXSTORAGE.INIT, "User Init Scripts");
    SPECIFIC_FIELD(lang->MUXSTORAGE.MUSIC, "Background Music");
    SPECIFIC_FIELD(lang->MUXSTORAGE.NAME, "Friendly Name System");
    SPECIFIC_FIELD(lang->MUXSTORAGE.NETWORK, "Network Profiles");
    SPECIFIC_FIELD(lang->MUXSTORAGE.PACKAGE, "Custom Packages");
    SPECIFIC_FIELD(lang->MUXSTORAGE.SAVE, "Save Games + Save States");
    SPECIFIC_FIELD(lang->MUXSTORAGE.SCREENSHOT, "Screenshots");
    SPECIFIC_FIELD(lang->MUXSTORAGE.SYNCTHING, "Syncthing Configs");
    SPECIFIC_FIELD(lang->MUXSTORAGE.THEME, "Themes");
    SPECIFIC_FIELD(lang->MUXSTORAGE.TRACK, "Activity Tracker");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.APPS, "Location of installed applications");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.BIOS, "Location of system BIOS files");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.CATALOGUE, "Location of content images and text");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.COLLECTION, "Location of content collection");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.HISTORY, "Location of history");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.INIT, "Location of User Initialisation scripts");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.MUSIC, "Location of background music");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.NAME, "Location of friendly name configurations");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.NETWORK, "Location of Network Profiles");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.PACKAGE, "Location of custom packages");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.SAVE, "Location of save states and files");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.SCREENSHOT, "Location of screenshots");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.SYNCTHING, "Location of Syncthing configurations");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.THEME, "Location of themes");
    SPECIFIC_FIELD(lang->MUXSTORAGE.HELP.TRACK, "Location of activity tracker");

    // muxsysinfo
    SPECIFIC_FIELD(lang->MUXSYSINFO.TITLE, "SYSTEM DETAILS");
    SPECIFIC_FIELD(lang->MUXSYSINFO.VERSION, "Version");
    SPECIFIC_FIELD(lang->MUXSYSINFO.BUILD, "Build ID");
    SPECIFIC_FIELD(lang->MUXSYSINFO.DEVICE, "Device Type");
    SPECIFIC_FIELD(lang->MUXSYSINFO.KERNEL, "Linux Kernel");
    SPECIFIC_FIELD(lang->MUXSYSINFO.UPTIME, "System Uptime");
    SPECIFIC_FIELD(lang->MUXSYSINFO.MEMORY.INFO, "System Memory");
    SPECIFIC_FIELD(lang->MUXSYSINFO.MEMORY.DROP, "Memory Cache Dropped");
    SPECIFIC_FIELD(lang->MUXSYSINFO.TEMP, "Temperature");
    SPECIFIC_FIELD(lang->MUXSYSINFO.CAPACITY, "Battery Capacity");
    SPECIFIC_FIELD(lang->MUXSYSINFO.VOLTAGE, "Battery Voltage");
    SPECIFIC_FIELD(lang->MUXSYSINFO.CHARGER, "Charger");
    SPECIFIC_FIELD(lang->MUXSYSINFO.REFRESH, "Refresh Frontend");
    SPECIFIC_FIELD(lang->MUXSYSINFO.CPU.INFO, "CPU Information");
    SPECIFIC_FIELD(lang->MUXSYSINFO.CPU.SPEED, "CPU Speed");
    SPECIFIC_FIELD(lang->MUXSYSINFO.CPU.GOVERNOR, "CPU Governor");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.VERSION, "The current version of MustardOS running on the device");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.BUILD, "The current build ID of MustardOS running on the device");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.DEVICE, "The current device type detected and configured");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.KERNEL, "The current Linux kernel");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.UPTIME, "The current running time of the system");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.MEMORY, "The current, and total, memory usage of the device");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.TEMP, "The current detected temperature of the device");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.CAPACITY, "The current detected battery capacity");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.VOLTAGE, "The current detected battery voltage");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.CHARGER, "Detection of the charger cable");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.REFRESH, "Refresh the current frontend configuration values if changed elsewhere");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.CPU.INFO, "The detected CPU type of the device");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.CPU.SPEED, "The current CPU frequency of the device");
    SPECIFIC_FIELD(lang->MUXSYSINFO.HELP.CPU.GOVERNOR, "The current running governor of the device");

    // muxtag
    SPECIFIC_FIELD(lang->MUXTAG.TITLE, "TAG");
    SPECIFIC_FIELD(lang->MUXTAG.HELP, "Change the tag of your current selected content to make it stand out");
    SPECIFIC_FIELD(lang->MUXTAG.NONE, "No Tags Found…");

    // muxtask
    SPECIFIC_FIELD(lang->MUXTASK.TITLE, "TASK TOOLKIT");
    SPECIFIC_FIELD(lang->MUXTASK.NONE, "No Tasks Found");

    // muxtester
    SPECIFIC_FIELD(lang->MUXTESTER.TITLE, "INPUT TESTER");
    SPECIFIC_FIELD(lang->MUXTESTER.ANY, "Press any button to start input testing!");
    SPECIFIC_FIELD(lang->MUXTESTER.QUIT, "Press DOWN + B to finish testing");
    SPECIFIC_FIELD(lang->MUXTESTER.QUIT_ALT, "Press DOWN + A to finish testing");

    // muxthemedown
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.TITLE, "THEME DOWNLOAD");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.REFRESH, "Refresh");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.REFRESH_RUN, "Refreshing…");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.THEME_REMOVED, "Theme Removed");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.NONE, "No Content Found…");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.DOWNLOAD, "Download");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.DOWN.THEME, "Downloading Theme");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.DOWN.DATA, "Downloading Theme Data");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.DOWN.PREVIEW, "Downloading Theme Previews");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.REMOVE, "Remove");
    SPECIFIC_FIELD(lang->MUXTHEMEDOWN.ERROR_GET_DATA, "Error Retrieving Theme Data");

    // muxthemefilter
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.TITLE, "THEME FILTER");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.COMPATIBILITY, "Theme Compatibility");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.COMPAT.DEVICE, "Device");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.COMPAT.ALL, "All");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.GRID, "Grid");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.HDMI, "HDMI");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.LANGUAGE, "Language");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.LOOKUP, "Lookup");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.HELP.COMPATIBILITY, "Filter to themes for this device or all themes. All themes can work on any device but if the theme does not implement your devices resolution it will be letterboxed.");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.HELP.GRID, "Filter to themes that support displaying content folders in a tile layout");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.HELP.HDMI, "Filter to themes that support HDMI resolution 1280x720. Themes will still work on HDMI without support for 1280x720 but content will be letterboxed.");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.HELP.LANGUAGE, "Filter to themes that let MustardOS handle translating text. Themes that use static images for the main menu will be filtered out.");
    SPECIFIC_FIELD(lang->MUXTHEMEFILTER.HELP.LOOKUP, "Filter to theme with a name containing lookup text.");

    // muxtimezone
    SPECIFIC_FIELD(lang->MUXTIMEZONE.TITLE, "TIMEZONE");
    SPECIFIC_FIELD(lang->MUXTIMEZONE.NONE, "No Timezones Found…");
    SPECIFIC_FIELD(lang->MUXTIMEZONE.SAVE, "Saving Timezone");
    SPECIFIC_FIELD(lang->MUXTIMEZONE.HELP, "Select your preferred timezone");

    // muxtweakadv
    SPECIFIC_FIELD(lang->MUXTWEAKADV.TITLE, "ADVANCED SETTINGS");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.SPEED, "Menu Acceleration");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.REPEAT_DELAY, "Menu Repeat Delay");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.THERMAL, "Thermal Zone Control");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.OFFSET, "Battery Offset");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.LOCK, "Passcode Lock");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.LED, "LED During Play");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RANDOM, "Random Theme on Boot");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.NET_WAIT, "RetroArch Network Wait");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RA_FREE, "RetroArch Config Freedom");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.VERBOSE, "Verbose Messages");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.USER_INIT, "User Init Scripts");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.DPAD, "DPAD Swap Function");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.OVERDRIVE, "Audio Overdrive");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.SWAPFILE, "System Swapfile");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.ZRAMFILE, "System ZRam");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.LIDSWITCH, "Device Lid Switch");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.SWAP.TITLE, "Button Swap");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.SWAP.RETRO, "Retro");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.SWAP.MODERN, "Modern");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.VOLUME.TITLE, "Volume On Boot");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.VOLUME.SILENT, "Silent");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.VOLUME.SOFT, "Soft");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.VOLUME.LOUD, "Loud");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.BRIGHT.TITLE, "Brightness On Boot");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.BRIGHT.LOW, "Low");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.BRIGHT.MEDIUM, "Medium");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.BRIGHT.HIGH, "High");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.TITLE, "Device Rumble");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.ST, "Startup");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.SH, "Shutdown");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.SL, "Sleep");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.STSH, "Startup + Shutdown");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.STSL, "Startup + Sleep");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.RUMBLE.SHSL, "Shutdown + Sleep");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.DISPSUSPEND, "Display Suspend");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.SECONDPART, "Secondary Partition");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.USBPART, "External Partition");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.INCBRIGHT, "Brightness Increment");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.INCVOLUME, "Volume Increment");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.MAXGPU, "GPU Performance Mode");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.AUDIOREADY, "Audio Subsystem Wait");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.SPEED, "Adjust the rate of speed when holding navigation keys down");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.REPEAT_DELAY, "Adjust amount of time button must be held before it begins to repeat the button action");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.THERMAL, "Toggle the system ability to automatically shut the device down due high temperature");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.OFFSET, "Change the displayed battery percentage to improve accuracy based on calibration or known deviations in the battery capacity reading");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.LOCK, "Toggle the passcode lock - More information can be found on the MustardOS website");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.LED, "Toggle the power LED during content launch");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.RANDOM, "Change the default theme used for the next device launch");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.NET_WAIT, "Toggle a delayed start of RetroArch until a network connection is established");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.RA_FREE, "Toggle the forced settings MustardOS places on RetroArch configurations");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.VERBOSE, "Toggle startup and shutdown verbose messages used for debugging faults");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.USER_INIT, "Toggle the functionality of the user initialisation scripts on device startup");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.DPAD, "Toggle the functionality of the power button to switch DPAD mode");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.OVERDRIVE, "Toggle the audio overdrive moving it from 100% to 200%");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.SWAPFILE, "Adjust the system swapfile if required by certain content");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.ZRAMFILE, "Adjust the system zram if required by certain content");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.LIDSWITCH, "Toggle the lid switch functionality for the device");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.RUMBLE, "Toggle vibration for device startup, sleep, and shutdown");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.BRIGHT, "Change the default brightness level that the device will use each time it starts up");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.VOLUME, "Change the default audio level that the device will use each time it starts up");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.SWAP, "Change how the device buttons work globally");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.DISPSUSPEND, "Toggle the device display suspend function, however some displays will not like this enabled");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.SECONDPART, "Change the partition number requested upon secondary storage mount");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.USBPART, "Change the partition number requested upon external storage mount");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.INCBRIGHT, "Change the level of brightness incrementation when adjusting");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.INCVOLUME, "Change the level of volume incrementation when adjusting");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.MAXGPU, "Push the onboard GPU to the maximum frequency at all times");
    SPECIFIC_FIELD(lang->MUXTWEAKADV.HELP.AUDIOREADY, "Toggle if the device will wait for the audio subsystem to initialise during boot");

    // muxtweakgen
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.TITLE, "GENERAL SETTINGS");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.DATETIME, "Date and Time");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.TEMP, "Colour Temperature");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.TEMP_SET, "Setting Colour Temperature");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.BRIGHT, "Brightness");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.BRIGHT_SET, "Setting Brightness");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.VOLUME, "Volume");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.VOLUME_SET, "Setting Volume");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HDMI, "HDMI Output");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.ADVANCED, "Advanced Settings");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.RGB, "Device RGB Lights");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HKDPAD, "DPAD Swap Hotkey");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HKSHOT, "Screenshot Hotkey");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.TITLE, "Device Startup");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.MENU, "Main Menu");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.EXPLORE, "Content Explorer");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.COLLECTION, "Content Collection");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.HISTORY, "History");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.LAST, "Last Game");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.STARTUP.RESUME, "Resume Game");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.DATETIME, "Change your current date, time, and timezone");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.TEMP, "Change the colour temperature of the display if the device supports it");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.BRIGHT, "Change the brightness of the device to a specific level");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.VOLUME, "Change the volume of the device to a specific level");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.HDMI, "Settings to change the HDMI output of the device");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.ADVANCED, "Settings that should only be changed by those who know what they are doing!");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.RGB, "Toggle the device fancy RGB light system");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.HKDPAD, "Switch between different hotkeys for toggling DPAD swap");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.HKSHOT, "Switch between different hotkeys for taking a screenshot");
    SPECIFIC_FIELD(lang->MUXTWEAKGEN.HELP.STARTUP, "Change where the device will start up into");

    // muxvisual
    SPECIFIC_FIELD(lang->MUXVISUAL.TITLE, "INTERFACE OPTIONS");
    SPECIFIC_FIELD(lang->MUXVISUAL.BATTERY, "Battery");
    SPECIFIC_FIELD(lang->MUXVISUAL.NETWORK, "Network");
    SPECIFIC_FIELD(lang->MUXVISUAL.CLOCK, "Clock");
    SPECIFIC_FIELD(lang->MUXVISUAL.DASH, "Content Dash Replacement");
    SPECIFIC_FIELD(lang->MUXVISUAL.FRIENDLY, "Friendly Folder Names");
    SPECIFIC_FIELD(lang->MUXVISUAL.REFORMAT, "Display Title Reformatting");
    SPECIFIC_FIELD(lang->MUXVISUAL.ROOT, "Title Include Root Drive");
    SPECIFIC_FIELD(lang->MUXVISUAL.COUNT, "Folder Item Count");
    SPECIFIC_FIELD(lang->MUXVISUAL.EMPTY, "Empty Folders");
    SPECIFIC_FIELD(lang->MUXVISUAL.COUNT_FOLDER, "Menu Counter Folder");
    SPECIFIC_FIELD(lang->MUXVISUAL.COUNT_FILE, "Menu Counter File");
    SPECIFIC_FIELD(lang->MUXVISUAL.NAME.TITLE, "Content Name Scheme");
    SPECIFIC_FIELD(lang->MUXVISUAL.NAME.FULL, "Full Name");
    SPECIFIC_FIELD(lang->MUXVISUAL.NAME.REM_SQ, "Remove [ ]");
    SPECIFIC_FIELD(lang->MUXVISUAL.NAME.REM_PA, "Remove ( )");
    SPECIFIC_FIELD(lang->MUXVISUAL.NAME.REM_SQPA, "Remove [ ] and ( )");
    SPECIFIC_FIELD(lang->MUXVISUAL.HIDDEN, "Show Hidden Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.CONTENTCOLLECT, "Collection In Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.CONTENTHISTORY, "History In Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.IMAGE, "Overlay Image");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.TRANSPARENCY, "Overlay Transparency");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.THEME, "Theme Provided");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.CHECKERBOARD.T1, "Checkerboard (1px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.CHECKERBOARD.T4, "Checkerboard (4px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.DIAGONAL.T1, "Diagonal Lines (1px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.DIAGONAL.T2, "Diagonal Lines (2px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.DIAGONAL.T4, "Diagonal Lines (4px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.LATTICE.T1, "Dot Lattice (1px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.LATTICE.T4, "Dot Lattice (4px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.HORIZONTAL.T1, "Horizontal Lines (1px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.HORIZONTAL.T2, "Horizontal Lines (2px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.HORIZONTAL.T4, "Horizontal Lines (4px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.VERTICAL.T1, "Vertical Lines (1px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.VERTICAL.T2, "Vertical Lines (2px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.OVERLAY.VERTICAL.T4, "Vertical Lines (4px)");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.BATTERY, "Toggle the visibility of the battery glyph");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.NETWORK, "Toggle the visibility of the network glyph");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.CLOCK, "Toggle the visibility of the clock");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.OVERLAY_IMAGE, "Switch between different overlay styles for the frontend only");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.OVERLAY_TRANSPARENCY, "Changes the transparency of the overlay image for the frontend only");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.DASH, "Replaces the dash (-) with a colon (:) for content labels");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.FRIENDLY, "Replaces the label of shortened content folders to more appropriately named labels");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.REFORMAT, "Rearranges the label of content to move the 'The' label to the front - For example, 'Batman and Robin, The' to 'The Batman and Robin'");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.ROOT, "Changes the top title label in Explore Content to show current storage device along with folder name");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.COUNT, "Toggle the visibility of the item count within folders in Explore Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.EMPTY, "Toggle the visibility of empty folders in Explore Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.COUNT_FOLDER, "Toggle the visibility of currently selected folder along with total in Explore Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.COUNT_FILE, "Toggle the visibility of currently selected file along with total in Explore Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.NAME, "Remove extra information from content labels - This does NOT rename your files it only changes how it is displayed");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.HIDDEN, "Toggle hidden content displayed in Explore Content - Place a '.' or '_' character at the start of a file or folder name to hide it");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.CONTENTCOLLECT, "Toggle the collection visibility withing Explore Content");
    SPECIFIC_FIELD(lang->MUXVISUAL.HELP.CONTENTHISTORY, "Toggle the history visibility within Explore Content");

    // muxwebserv
    SPECIFIC_FIELD(lang->MUXWEBSERV.TITLE, "WEB SERVICES");
    SPECIFIC_FIELD(lang->MUXWEBSERV.NTP, "Network Time Sync");
    SPECIFIC_FIELD(lang->MUXWEBSERV.TERMINAL, "Virtual Terminal");
    SPECIFIC_FIELD(lang->MUXWEBSERV.SYNCTHING, "Syncthing");
    SPECIFIC_FIELD(lang->MUXWEBSERV.SHELL, "Secure Shell");
    SPECIFIC_FIELD(lang->MUXWEBSERV.SFTP, "SFTP + Filebrowser");
    SPECIFIC_FIELD(lang->MUXWEBSERV.TAILSCALE, "Tailscale");
    SPECIFIC_FIELD(lang->MUXWEBSERV.HELP.NTP, "Toggle network time protocol for active network connections");
    SPECIFIC_FIELD(lang->MUXWEBSERV.HELP.TERMINAL, "Toggle virtual terminal - WebUI can be found on port 8080");
    SPECIFIC_FIELD(lang->MUXWEBSERV.HELP.SYNCTHING, "Toggle Syncthing - WebUI can be found on port 7070");
    SPECIFIC_FIELD(lang->MUXWEBSERV.HELP.SHELL, "Toggle SSH support - Access via port 22");
    SPECIFIC_FIELD(lang->MUXWEBSERV.HELP.SFTP, "Toggle SFTP support - WebUI can be found on port 9090");
    SPECIFIC_FIELD(lang->MUXWEBSERV.HELP.TAILSCALE, "Toggle Tailscale - Need to login via SSH first to configure it");

#undef SYSTEM_FIELD
#undef GENERIC_FIELD
#undef SPECIFIC_FIELD
}
