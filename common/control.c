#include <stdio.h>
#include <limits.h>
#include "init.h"
#include "fileio.h"
#include "ini.h"
#include "options.h"
#include "control.h"
#include "log.h"
#include "mini/mini.h"

void create_controller_profile(char *controller_profile_path) {
    LOG_WARN(mux_module, "Controller profile missing: %s", controller_profile_path);

    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", controller_profile_path) >= (int) sizeof(tmp)) {
        LOG_ERROR(mux_module, "Failed to write controller profile: %s", controller_profile_path);
        return;
    }

    FILE *file = fopen(tmp, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "Failed to write controller profile: %s", controller_profile_path);
        return;
    }

    fprintf(file, "[buttons]\n");
    fprintf(file, "BUTTON_A=0\n");
    fprintf(file, "BUTTON_B=1\n");
    fprintf(file, "BUTTON_X=3\n");
    fprintf(file, "BUTTON_Y=4\n");
    fprintf(file, "BUTTON_L1=6\n");
    fprintf(file, "BUTTON_L2=8\n");
    fprintf(file, "BUTTON_L3=13\n");
    fprintf(file, "BUTTON_R1=7\n");
    fprintf(file, "BUTTON_R2=9\n");
    fprintf(file, "BUTTON_R3=14\n");
    fprintf(file, "BUTTON_SELECT=10\n");
    fprintf(file, "BUTTON_START=11\n");
    fprintf(file, "BUTTON_MENU=12\n");
    fprintf(file, "BUTTON_UP=-1\n");
    fprintf(file, "BUTTON_DOWN=-1\n");
    fprintf(file, "BUTTON_LEFT=-1\n");
    fprintf(file, "BUTTON_RIGHT=-1\n\n");

    fprintf(file, "[trigger]\n");
    fprintf(file, "AXIS=32767\n");
    fprintf(file, "L2=-1\n");
    fprintf(file, "R2=-1\n\n");

    fprintf(file, "[dpad]\n");
    fprintf(file, "AXIS=32767\n");
    fprintf(file, "LEFT=6\n");
    fprintf(file, "UP=7\n\n");

    fprintf(file, "[analog_left]\n");
    fprintf(file, "AXIS=32767\n");
    fprintf(file, "LEFT=0\n");
    fprintf(file, "UP=1\n\n");

    fprintf(file, "[analog_right]\n");
    fprintf(file, "AXIS=32767\n");
    fprintf(file, "LEFT=2\n");
    fprintf(file, "UP=3\n");

    const int ok = fflush(file) == 0 && fsync(fileno(file)) == 0;
    fclose(file);

    if (!ok || rename(tmp, controller_profile_path) != 0) {
        remove(tmp);
        LOG_ERROR(mux_module, "Failed to write controller profile: %s", controller_profile_path);
    }
}

void load_controller_profile(struct control *controller, char *controller_name) {
    char controller_profile_path[MAX_BUFFER_SIZE];
    snprintf(controller_profile_path, sizeof(controller_profile_path), INFO_CNT_PATH "/%s.ini", controller_name);

    LOG_SUCCESS(mux_module, "Loading Controller Profile: %s", controller_profile_path);

    if (!file_exist(controller_profile_path)) {
        create_controller_profile(controller_profile_path);
    }

    if (!file_exist(controller_profile_path)) return;

    mini_t *muos_controller_profile = mini_try_load(controller_profile_path);

    controller->button.a = get_ini_int(muos_controller_profile, "buttons", "BUTTON_A", 0);
    controller->button.b = get_ini_int(muos_controller_profile, "buttons", "BUTTON_B", 1);
    controller->button.x = get_ini_int(muos_controller_profile, "buttons", "BUTTON_X", 3);
    controller->button.y = get_ini_int(muos_controller_profile, "buttons", "BUTTON_Y", 4);
    controller->button.l1 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_L1", 6);
    controller->button.l2 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_L2", 8);
    controller->button.l3 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_L3", 13);
    controller->button.r1 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_R1", 7);
    controller->button.r2 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_R2", 9);
    controller->button.r3 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_R3", 14);
    controller->button.select = get_ini_int(muos_controller_profile, "buttons", "BUTTON_SELECT", 10);
    controller->button.start = get_ini_int(muos_controller_profile, "buttons", "BUTTON_START", 11);
    controller->button.menu = get_ini_int(muos_controller_profile, "buttons", "BUTTON_MENU", 12);
    controller->button.up = get_ini_int(muos_controller_profile, "buttons", "BUTTON_UP", -1);
    controller->button.down = get_ini_int(muos_controller_profile, "buttons", "BUTTON_DOWN", -1);
    controller->button.left = get_ini_int(muos_controller_profile, "buttons", "BUTTON_LEFT", -1);
    controller->button.right = get_ini_int(muos_controller_profile, "buttons", "BUTTON_RIGHT", -1);

    controller->trigger.axis = get_ini_int(muos_controller_profile, "trigger", "AXIS", 32767);
    controller->trigger.l2 = get_ini_int(muos_controller_profile, "trigger", "L2", -1);
    controller->trigger.r2 = get_ini_int(muos_controller_profile, "trigger", "R2", -1);

    controller->dpad.axis = get_ini_int(muos_controller_profile, "dpad", "AXIS", 32767);
    controller->dpad.left = get_ini_int(muos_controller_profile, "dpad", "LEFT", 6);
    controller->dpad.up = get_ini_int(muos_controller_profile, "dpad", "UP", 7);

    controller->analog.left.axis = get_ini_int(muos_controller_profile, "analog_left", "AXIS", 32767);
    controller->analog.left.left = get_ini_int(muos_controller_profile, "analog_left", "LEFT", 0);
    controller->analog.left.up = get_ini_int(muos_controller_profile, "analog_left", "UP", 1);

    controller->analog.right.axis = get_ini_int(muos_controller_profile, "analog_right", "AXIS", 32767);
    controller->analog.right.left = get_ini_int(muos_controller_profile, "analog_right", "LEFT", 2);
    controller->analog.right.up = get_ini_int(muos_controller_profile, "analog_right", "UP", 3);

    mini_free(muos_controller_profile);
}
