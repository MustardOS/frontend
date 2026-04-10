#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "passcode.h"
#include "log.h"
#include "mini/mini.h"

void load_passcode(struct mux_passcode *passcode) {
    const char *pass_file = resolve_info_path("pass.ini");

    if (!pass_file) {
        LOG_ERROR(mux_module, "Password configuration not found");
        exit(1);
    }

    mini_t *muos_pass = mini_try_load(pass_file);
    if (!muos_pass) {
        LOG_ERROR(mux_module, "Password configuration could not be loaded: %s", pass_file);
        exit(1);
    }

    strncpy(passcode->CODE.BOOT, get_ini_string(muos_pass, "code", "boot", "000000"), MAX_BUFFER_SIZE - 1);
    passcode->CODE.BOOT[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(passcode->CODE.LAUNCH, get_ini_string(muos_pass, "code", "launch", "000000"), MAX_BUFFER_SIZE - 1);
    passcode->CODE.LAUNCH[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(passcode->CODE.SETTING, get_ini_string(muos_pass, "code", "setting", "000000"), MAX_BUFFER_SIZE - 1);
    passcode->CODE.SETTING[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(passcode->MESSAGE.BOOT, get_ini_string(muos_pass, "message", "boot", ""), MAX_BUFFER_SIZE - 1);
    passcode->MESSAGE.BOOT[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(passcode->MESSAGE.LAUNCH, get_ini_string(muos_pass, "message", "launch", ""), MAX_BUFFER_SIZE - 1);
    passcode->MESSAGE.LAUNCH[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(passcode->MESSAGE.SETTING, get_ini_string(muos_pass, "message", "setting", ""), MAX_BUFFER_SIZE - 1);
    passcode->MESSAGE.SETTING[MAX_BUFFER_SIZE - 1] = '\0';

    mini_free(muos_pass);
}
