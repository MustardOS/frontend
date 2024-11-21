#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "controller_profile.h"
#include "mini/mini.h"

void create_controller_profile(char *controller_profile_path) {
    printf("file missing: %s\n", controller_profile_path);
    FILE *file = fopen(controller_profile_path, "w");

    if (file == NULL) {
        perror("Error opening file for writing");
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
    fprintf(file, "BUTTON_MENU=12\n\n");

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
    fprintf(file, "UP=3\n\n");

    fclose(file);
}

void load_controller_profile(struct controller_profile *controller, char *controller_name) {
    char controller_profile_path[MAX_BUFFER_SIZE];
    snprintf(controller_profile_path, sizeof(controller_profile_path), "%s/%s.ini",
                INFO_CNT_PATH, controller_name);

    printf("Loading Controller Profile: %s\n", controller_profile_path);

    if (!file_exist(controller_profile_path)) {
        create_controller_profile(controller_profile_path);
    }
    
    if (!file_exist(controller_profile_path)) return;
    
    mini_t *muos_controller_profile = mini_try_load(controller_profile_path);

    controller->BUTTON.A = get_ini_int(muos_controller_profile, "buttons", "BUTTON_A", 0);
    controller->BUTTON.B = get_ini_int(muos_controller_profile, "buttons", "BUTTON_B", 1);
    controller->BUTTON.X = get_ini_int(muos_controller_profile, "buttons", "BUTTON_X", 3);
    controller->BUTTON.Y = get_ini_int(muos_controller_profile, "buttons", "BUTTON_Y", 4);
    controller->BUTTON.L1 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_L1", 6);
    controller->BUTTON.L2 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_L2", 8);
    controller->BUTTON.L3 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_L3", 13);
    controller->BUTTON.R1 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_R1", 7);
    controller->BUTTON.R2 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_R2", 9);
    controller->BUTTON.R3 = get_ini_int(muos_controller_profile, "buttons", "BUTTON_R3", 14);
    controller->BUTTON.SELECT = get_ini_int(muos_controller_profile, "buttons", "BUTTON_SELECT", 10);
    controller->BUTTON.START = get_ini_int(muos_controller_profile, "buttons", "BUTTON_START", 11);
    controller->BUTTON.MENU = get_ini_int(muos_controller_profile, "buttons", "BUTTON_MENU", 12);
    
    controller->TRIGGER.AXIS = get_ini_int(muos_controller_profile, "trigger", "AXIS", 32767);
    controller->TRIGGER.L2 = get_ini_int(muos_controller_profile, "trigger", "L2", -1);
    controller->TRIGGER.R2 = get_ini_int(muos_controller_profile, "trigger", "R2", -1);

    controller->DPAD.AXIS = get_ini_int(muos_controller_profile, "dpad", "AXIS", 32767);
    controller->DPAD.LEFT = get_ini_int(muos_controller_profile, "dpad", "LEFT", 6);
    controller->DPAD.UP = get_ini_int(muos_controller_profile, "dpad", "UP", 7);

    controller->ANALOG.LEFT.AXIS = get_ini_int(muos_controller_profile, "analog_left", "AXIS", 32767);
    controller->ANALOG.LEFT.LEFT = get_ini_int(muos_controller_profile, "analog_left", "LEFT", 0);
    controller->ANALOG.LEFT.UP = get_ini_int(muos_controller_profile, "analog_left", "UP", 1);

    controller->ANALOG.RIGHT.AXIS = get_ini_int(muos_controller_profile, "analog_right", "AXIS", 32767);
    controller->ANALOG.RIGHT.LEFT = get_ini_int(muos_controller_profile, "analog_right", "LEFT", 2);
    controller->ANALOG.RIGHT.UP = get_ini_int(muos_controller_profile, "analog_right", "UP", 3);

    mini_free(muos_controller_profile);
}
