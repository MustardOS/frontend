#include "options.h"
#include "fileio.h"
#include "passcode.h"
#include <stdio.h>
#include <stdlib.h>

#define PASSCODE_CFG_PATH CONF_CONFIG_PATH "passcode/"

static void load_code_file(char *dst, const char *filename, const char *fallback) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s%s", PASSCODE_CFG_PATH, filename);

    char *val = read_line_char_from(path, 1);
    snprintf(dst, MAX_BUFFER_SIZE, "%s", val && val[0] != '\0' ? val : fallback);
    free(val);
}

void load_passcode(struct mux_passcode *passcode) {
    load_code_file(passcode->code.boot, "code_boot", "000000");
    load_code_file(passcode->code.launch, "code_launch", "000000");
    load_code_file(passcode->code.setting, "code_setting", "000000");
    load_code_file(passcode->code.safety, "code_safety", "000000");
    load_code_file(passcode->message.boot, "message_boot", "");
    load_code_file(passcode->message.launch, "message_launch", "");
    load_code_file(passcode->message.setting, "message_setting", "");
}

void save_passcode(struct mux_passcode *passcode) {
    char path[MAX_BUFFER_SIZE];

    snprintf(path, sizeof(path), "%scode_boot", PASSCODE_CFG_PATH);
    create_directories(path, 1);
    write_text_to_file_atomic(path, CHAR, passcode->code.boot);

    snprintf(path, sizeof(path), "%scode_launch", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->code.launch);

    snprintf(path, sizeof(path), "%scode_setting", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->code.setting);

    snprintf(path, sizeof(path), "%scode_safety", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->code.safety);

    snprintf(path, sizeof(path), "%smessage_boot", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->message.boot);

    snprintf(path, sizeof(path), "%smessage_launch", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->message.launch);

    snprintf(path, sizeof(path), "%smessage_setting", PASSCODE_CFG_PATH);
    write_text_to_file_atomic(path, CHAR, passcode->message.setting);
}
