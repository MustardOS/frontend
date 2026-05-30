#include "common.h"
#include "options.h"
#include "passcode.h"

#define PASSCODE_CFG_PATH CONF_CONFIG_PATH "passcode/"

static void load_code_file(char *dst, const char *filename, const char *fallback) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s%s", PASSCODE_CFG_PATH, filename);

    const char *val = read_line_char_from(path, 1);
    snprintf(dst, MAX_BUFFER_SIZE, "%s", (val && val[0] != '\0') ? val : fallback);
}

void load_passcode(struct mux_passcode *passcode) {
    load_code_file(passcode->CODE.BOOT, "code_boot", "000000");
    load_code_file(passcode->CODE.LAUNCH, "code_launch", "000000");
    load_code_file(passcode->CODE.SETTING, "code_setting", "000000");
    load_code_file(passcode->CODE.SAFETY, "code_safety", "000000");
    load_code_file(passcode->MESSAGE.BOOT, "message_boot", "");
    load_code_file(passcode->MESSAGE.LAUNCH, "message_launch", "");
    load_code_file(passcode->MESSAGE.SETTING, "message_setting", "");
}

void save_passcode(struct mux_passcode *passcode) {
    char path[MAX_BUFFER_SIZE];

    snprintf(path, sizeof(path), "%scode_boot", PASSCODE_CFG_PATH);
    create_directories(path, 1);
    write_text_to_file_atomic(path, CHAR, passcode->CODE.BOOT);

    snprintf(path, sizeof(path), "%scode_launch", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->CODE.LAUNCH);

    snprintf(path, sizeof(path), "%scode_setting", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->CODE.SETTING);

    snprintf(path, sizeof(path), "%scode_safety", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->CODE.SAFETY);

    snprintf(path, sizeof(path), "%smessage_boot", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->MESSAGE.BOOT);

    snprintf(path, sizeof(path), "%smessage_launch", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->MESSAGE.LAUNCH);

    snprintf(path, sizeof(path), "%smessage_setting", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->MESSAGE.SETTING);
}
