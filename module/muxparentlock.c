#include "muxshare.h"
#include "muxparentlock.h"
#include "ui/ui_muxparentlock.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/parentlock.h"

static int exit_status_muxparentlock = 0;

struct mux_parentlock parentlock;
static char *p_code;
static char *p_msg;

#define UI_COUNT 4
static lv_obj_t *ui_objects[UI_COUNT];

static lv_obj_t *ui_mux_panels[2];

static void init_navigation_group() {
    ui_objects[0] = ui_plrolComboOne;
    ui_objects[1] = ui_plrolComboTwo;
    ui_objects[2] = ui_plrolComboThree;
    ui_objects[3] = ui_plrolComboFour;

    ui_group = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }
}

static void handle_confirm(void) {
    play_sound(SND_CONFIRM, 0);

    char b1[2], b2[2], b3[2], b4[2];
    uint32_t bs = sizeof(b1);

    lv_roller_get_selected_str(ui_plrolComboOne, b1, bs);
    lv_roller_get_selected_str(ui_plrolComboTwo, b2, bs);
    lv_roller_get_selected_str(ui_plrolComboThree, b3, bs);
    lv_roller_get_selected_str(ui_plrolComboFour, b4, bs);

    char try_code[13];
    sprintf(try_code, "%s%s%s%s", b1, b2, b3, b4);

    if (strcasecmp(try_code, p_code) == 0) {
        exit_status_muxparentlock = 1;
        close_input();
        mux_input_stop();
    }
}

static void handle_back(void) {
    play_sound(SND_BACK, 0);

    exit_status_muxparentlock = 2;
    close_input();
    mux_input_stop();
}

static void handle_up(void) {
    play_sound(SND_NAVIGATE, 0);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) - 1,
                           LV_ANIM_ON);
}

static void handle_down(void) {
    play_sound(SND_NAVIGATE, 0);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) + 1,
                           LV_ANIM_ON);
}

static void handle_left(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);
    nav_prev(ui_group, 1);
}

static void handle_right(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);
    nav_next(ui_group, 1);
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

int muxparentlock_main(char *p_type) {
    exit_status_muxparentlock = 0;
    char *cmd_help = "\nmuOS Extras - Parental Lock\nUsage: %s <-t>\n\nOptions:\n"
                     "\t-t Allowed uptime for today in min\n\n";

    init_module("muxparentlock");

    load_parentlock(&parentlock, &device);

    if (strcasecmp(p_type, "unlock") == 0) {
        p_code = parentlock.CODE.UNLOCK;
        p_msg = parentlock.MESSAGE.UNLOCK;
    } else {
        fprintf(stderr, cmd_help, p_type);
        return 2;
    }

    if (strcasecmp(p_code, "000000") == 0) {
        return 1;
    }

    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPARENTLOCK.TITLE);
    init_muxparentlock(ui_pnlContent);
    init_elements();

    if (strlen(p_msg) > 1) toast_message(p_msg, 0, 0);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    apply_parentlock_theme(ui_plrolComboOne, ui_plrolComboTwo, ui_plrolComboThree,
                     ui_plrolComboFour);

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    load_font_text(ui_screen);

    init_fonts();
    init_navigation_group();

    load_kiosk(&kiosk);

    init_timer(NULL, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            }
    };
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return exit_status_muxparentlock;
}

static int triggerLock()
{
	
}

int muxparentlock_process()
{
    struct timespec boot, cur;
    time_t lastBoot = 0;
    unsigned additionalTime = 0, maxTimeForToday = 86400;
    struct tm current;
	// Save startup time (using monotonic clock that's not counting while the device is suspended)
	clock_gettime( CLOCK_MONOTONIC, &boot );

	// Load the last boot time file, to avoid gremlins from shutting down the device to reset the counter
	char counter_file[MAX_BUFFER_SIZE];
	int written = snprintf(counter_file, sizeof(counter_file), "%s/%s/parent_ctr.txt", device->STORAGE.ROM.MOUNT, MUOS_INFO_PATH);

	if (written < 0 || (size_t) written >= sizeof(counter_file)) return triggerLock();

	// We need to know what day of week we are
	if (!localtime_r(time(NULL), &current)) return triggerLock();

    if (file_exist(counter_file)) {
		struct tm previous;
		char * prev_run = read_line_from_file(counter_file, 1);
		if (sscanf(prev_run, "%ull %u", &lastBoot, &additionalTime) != 2) { free(prev_run); return triggerLock(); }
		free(prev_run);

		if (!localtime_r(lastBoot, &previous)) return triggerLock();

		// Get current day of week and check if it's valid
		if (previous.tm_wday != current.tm_wday || previous.tm_mday != current.tm_mday || previous.tm_mon != current.tm_mon) {
			// Gremlins isn't cheating, let's clear the additional time
			additionalTime = 0;
		} 
		remove(counter_file);
	}		

	// Read configuration now to know what's the maximum allowed time for today
	load_parentlock(&parentlock, &device);
	switch (current.tm_wday)
	{
	case 0: // Sunday
		sscanf(parentlock.TIMES.SUNDAY, "%u", &maxTimeForToday);
		break;
	case 1: // Monday
		sscanf(parentlock.TIMES.MONDAY, "%u", &maxTimeForToday);
		break;
	case 2: // Tuesday
		sscanf(parentlock.TIMES.TUESDAY, "%u", &maxTimeForToday);
		break;
	case 3: // Wednesday
		sscanf(parentlock.TIMES.WEDNESDAY, "%u", &maxTimeForToday);
		break;
	case 4: // Thursday
		sscanf(parentlock.TIMES.THURSDAY, "%u", &maxTimeForToday);
		break;
	case 5: // Friday
		sscanf(parentlock.TIMES.FRIDAY, "%u", &maxTimeForToday);
		break;
	case 6: // Saturday
		sscanf(parentlock.TIMES.SATURDAY, "%u", &maxTimeForToday);
		break;
	default: return triggerLock();
	}

	// If 0: no limit else need to convert from min to sec
	maxTimeForToday = !maxTimeForToday ? 86400 : maxTimeForToday * 60;
	

	// Main process is dumb here, we are sleeping for 1mn and take the time, 
	// and write it to the counter or trigger the parental lock
	int throttleWrite = 0;
	while (1)
	{
		unsigned elapsed = 0;
		sleep(60);
		throttleWrite++;
		clock_gettime( CLOCK_MONOTONIC, &cur );

		elapsed = cur.tv_sec - boot.tv_sec + additionalTime;

		if (throttleWrite == 5) {
			FILE * file = fopen(counter_file, "w");
			if (!file) return triggerLock();
			fprintf(file, "%ull %u\n", lastBoot, elapsed);
			fclose(file);
		}

		if (elapsed > maxTimeForToday) return triggerLock();
	}
	return 0;
}
