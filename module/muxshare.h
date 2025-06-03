#pragma once

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/limits.h>

#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "../lvgl/lvgl.h"
#include "../common/init.h"
#include "../common/log.h"
#include "../common/options.h"
#include "../common/device.h"
#include "../common/config.h"
#include "../common/theme.h"
#include "../common/kiosk.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/osk.h"
#include "../common/common_core.h"
#include "../common/overlay.h"
#include "../common/language.h"
#include "../common/collection.h"
#include "../common/passcode.h"
#include "../common/timezone.h"
#include "../common/img/missing.h"
#include "../common/img/nothing.h"
#include "../common/input/list_nav.h"
#include "../common/json/json.h"
#include "../font/notosans_big.h"
#include "../font/notosans_big_hd.h"
#include "../lookup/lookup.h"

extern lv_obj_t *ui_mux_minimal_panels[2];
extern lv_obj_t *ui_mux_standard_panels[5];
extern lv_obj_t *ui_mux_extra_panels[7];

extern size_t item_count;
extern content_item *items;

extern int refresh_kiosk;
extern int refresh_config;
extern int refresh_resolution;

extern int bar_header;
extern int bar_footer;

extern int nav_moved;
extern int current_item_index;
extern int first_open;
extern int ui_count;

extern lv_obj_t *msgbox_element;
extern lv_obj_t *overlay_image;
extern lv_obj_t *kiosk_image;

extern int progress_onscreen;

extern lv_group_t *ui_group;
extern lv_group_t *ui_group_glyph;
extern lv_group_t *ui_group_panel;
extern lv_group_t *ui_group_value;

int muxapp_main();

int muxarchive_main();

int muxassign_main(int auto_assign, char *name, char *dir, char *sys);

int muxcollect_main(int add, char *dir, int last_index);

int muxconfig_main();

int muxconnect_main();

int muxcustom_main();

int muxgov_main(int auto_assign, char *name, char *dir, char *sys);

int muxhdmi_main();

int muxhistory_main(int his_index);

int muxinfo_main();

int muxkiosk_main();

int muxlanguage_main();

int muxlaunch_main();

int muxnetinfo_main();

int muxnetprofile_main();

int muxnetscan_main();

int muxnetwork_main();

int muxoption_main(int nothing, char *name, char *dir, char *sys);

int muxpass_main(char *p_type);

int muxpicker_main(char *type, char *ex_dir);

int muxplore_main(int index, char *dir);

int muxpower_main();

int muxrtc_main();

int muxsearch_main(char *dir);

int muxshot_main();

int muxspace_main();

int muxsplash_main(char *splash_image);

int muxstorage_main();

int muxsysinfo_main();

int muxtag_main(int nothing, char *name, char *dir, char *sys);

int muxtask_main(char *ex_dir);

int muxtester_main();

int muxtimezone_main();

int muxtweakadv_main();

int muxtweakgen_main();

int muxvisual_main();

int muxwebserv_main();
