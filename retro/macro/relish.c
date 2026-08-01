#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../common/ini.h"
#include "../../common/options.h"
#include "relish.h"

#define RELISH_GROUP          "relish_index"
#define RELISH_REGISTRY_FILE  ".relish_index"
#define RELISH_REGISTRY_MAX   MACRO_MAX
#define RELISH_LABEL_MAX      64
#define RELISH_LABEL_NAME_MAX 64
#define RELISH_KEYWORD_MAX    16
#define RELISH_LOOP_NEST_MAX  16
#define RELISH_LOOP_BLOCK_MAX MACRO_STEP_MAX
#define RELISH_BREAK_MAX      MACRO_STEP_MAX
#define RELISH_SAFETY_BUDGET  MACRO_CONTROL_BUDGET
#define RELISH_DEF_LENGTH     128
#define RELISH_ERROR_LENGTH   MACRO_ERROR_MAX
#define RELISH_DEFINE_MAX     32
#define RELISH_TEXT_MAX       256
#define RELISH_LINE_MAX       512
#define RELISH_SOURCE_MAX     8
#define RELISH_INCLUDE_DEPTH  4
#define RELISH_SOURCE_BYTES   (128 * 1024)
#define RELISH_AXIS_MAX       32767
#define RELISH_VALUE_MAX      65535

struct relish_label {
    char name[RELISH_LABEL_NAME_MAX];
    int index;
};

struct relish_break_ref {
    int line_no;
    int owning_depth;
    int target_index;
};

struct relish_define {
    char name[RELISH_LABEL_NAME_MAX];
    int32_t value;
};

struct relish_line {
    char text[RELISH_TEXT_MAX];
    int line_no;
    int source;
};

struct relish_program {
    struct relish_line lines[RELISH_LINE_MAX];
    int count;
    char sources[RELISH_SOURCE_MAX][MACRO_NAME_MAX];
    int source_count;
    size_t source_bytes;
    size_t bytes_read;
    long long source_mtime;
};

struct relish_compile {
    struct relish_program prog;
    struct relish_label labels[RELISH_LABEL_MAX];
    int label_count;
    int loop_instr_index[RELISH_LOOP_BLOCK_MAX];
    struct relish_break_ref breaks[RELISH_BREAK_MAX];
    int break_count;
    struct relish_define defines[RELISH_DEFINE_MAX];
    int define_count;
    char vars[MACRO_VAR_MAX][RELISH_LABEL_NAME_MAX];
    int var_count;
    int step_line[MACRO_STEP_MAX];
    int step_count;
};

struct relish_registry_entry {
    char file[MACRO_NAME_MAX];
    int index;
};

static struct relish_registry_entry registry[RELISH_REGISTRY_MAX];
static int registry_count = 0;
static int registry_dirty = 0;

static struct button_token {
    const char *name;
    int bit;
} button_tokens[] = {
    {"A", 8},   {"B", 0},   {"X", 9},     {"Y", 1},      {"L1", 10}, {"R1", 11},  {"L2", 12},  {"R2", 13},
    {"L3", 14}, {"R3", 15}, {"START", 3}, {"SELECT", 2}, {"UP", 4},  {"DOWN", 5}, {"LEFT", 6}, {"RIGHT", 7},
};
#define BUTTON_TOKEN_COUNT ((int) (sizeof(button_tokens) / sizeof(button_tokens[0])))

static int button_bit_for_token(const char *token) {
    for (int i = 0; i < BUTTON_TOKEN_COUNT; i++) {
        if (strcasecmp(token, button_tokens[i].name) == 0) return button_tokens[i].bit;
    }
    return -1;
}

static struct op_token {
    const char *name;
    int op;
} op_tokens[] = {
    {"EQUALS", if_op_equals},   {"NOTEQUALS", if_op_notequals}, {"LESS", if_op_less},
    {"GREATER", if_op_greater}, {"ATLEAST", if_op_atleast},     {"ATMOST", if_op_atmost},
};
#define OP_TOKEN_COUNT ((int) (sizeof(op_tokens) / sizeof(op_tokens[0])))

static int op_from_word(const char *word, int *out_op) {
    for (int i = 0; i < OP_TOKEN_COUNT; i++) {
        if (strcasecmp(word, op_tokens[i].name) == 0) {
            *out_op = op_tokens[i].op;
            return 0;
        }
    }
    return -1;
}

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void fallback_name_from_path(const char *path, char *out) {
    char stripped[MACRO_NAME_MAX];
    snprintf(stripped, sizeof(stripped), "%s", basename_of(path));

    char *dot = strrchr(stripped, '.');
    if (dot) *dot = '\0';

    snprintf(out, RELISH_DEF_LENGTH, "%s", stripped);
}

static void strip_crlf(char *line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
}

static void skip_ws(char **cursor) {
    while (**cursor == ' ' || **cursor == '\t')
        (*cursor)++;
}

static char *read_token(char *cursor, char *out, const size_t out_len) {
    size_t consumed = 0;
    size_t copied = 0;

    while (cursor[consumed] && !isspace((unsigned char) cursor[consumed])) {
        if (copied + 1 < out_len) out[copied++] = cursor[consumed];
        consumed++;
    }
    if (out_len > 0) out[copied] = '\0';
    return cursor + consumed;
}

static const char *line_label(const struct relish_program *prog, const int index) {
    static char buf[96];

    if (index < 0 || index >= prog->count) {
        snprintf(buf, sizeof(buf), "Line ?");
        return buf;
    }

    const struct relish_line *line = &prog->lines[index];
    if (line->source == 0) {
        snprintf(buf, sizeof(buf), "Line %d", line->line_no);
    } else {
        snprintf(buf, sizeof(buf), "Line %d of %.64s", line->line_no, prog->sources[line->source]);
    }

    return buf;
}

static const char *load_label(const int depth, const char *file_name, const int line_no) {
    static char buf[96];

    if (depth == 0) {
        snprintf(buf, sizeof(buf), "Line %d", line_no);
    } else {
        snprintf(buf, sizeof(buf), "Line %d of %.64s", line_no, file_name);
    }

    return buf;
}

static int program_add_source(struct relish_program *prog, const char *name) {
    for (int i = 0; i < prog->source_count; i++) {
        if (strcasecmp(prog->sources[i], name) == 0) return -1;
    }
    if (prog->source_count >= RELISH_SOURCE_MAX) return -2;

    snprintf(prog->sources[prog->source_count], MACRO_NAME_MAX, "%s", name);
    return prog->source_count++;
}

static int source_name_is_valid(const char *file_name, const int depth) {
    if (!file_name[0] || strchr(file_name, '/') || strchr(file_name, '\\')) return 0;

    const char *dot = strrchr(file_name, '.');
    if (!dot) return 0;
    return strcasecmp(dot, depth == 0 ? ".rls" : ".rli") == 0;
}

static int load_source(
    struct relish_program *prog, const int dir_fd, const char *file_name, const int depth, const char *from, char *error
) {
    if (depth > RELISH_INCLUDE_DEPTH) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: INCLUDE nesting is too deep (max %d)", from, RELISH_INCLUDE_DEPTH);
        return -1;
    }

    if (!source_name_is_valid(file_name, depth)) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: %s must be a plain %s file name", from,
            depth == 0 ? "script" : "INCLUDE", depth == 0 ? ".rls" : ".rli"
        );
        return -1;
    }

    const int source = program_add_source(prog, file_name);
    if (source == -1) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: '%s' is included more than once", from, file_name);
        return -1;
    }
    if (source == -2) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: too many INCLUDE files (max %d)", from, RELISH_SOURCE_MAX);
        return -1;
    }

    const int fd = openat(dir_fd, file_name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        if (depth == 0) {
            snprintf(error, RELISH_ERROR_LENGTH, "Could not open file");
        } else {
            snprintf(error, RELISH_ERROR_LENGTH, "%s: could not open included file '%s'", from, file_name);
        }
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
        close(fd);
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: '%s' must be a regular, non-linked file", from, file_name
        );
        return -1;
    }

    if (st.st_size < 0 || (size_t) st.st_size > RELISH_SOURCE_BYTES - prog->source_bytes) {
        close(fd);
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: script sources exceed %d KiB", from, RELISH_SOURCE_BYTES / 1024
        );
        return -1;
    }
    prog->source_bytes += (size_t) st.st_size;
    if (depth == 0) prog->source_mtime = (long long) st.st_mtime;

    FILE *f = fdopen(fd, "r");
    if (!f) {
        close(fd);
        snprintf(error, RELISH_ERROR_LENGTH, "%s: could not read '%s'", from, file_name);
        return -1;
    }

    char raw[MAX_BUFFER_SIZE];
    int line_no = 0;
    int result = 0;

    while (fgets(raw, sizeof(raw), f)) {
        const size_t bytes = strlen(raw);
        if (bytes > RELISH_SOURCE_BYTES - prog->bytes_read) {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: script sources exceed %d KiB", from, RELISH_SOURCE_BYTES / 1024
            );
            result = -1;
            break;
        }
        prog->bytes_read += bytes;
        line_no++;
        strip_crlf(raw);

        char *cursor = raw;
        skip_ws(&cursor);
        if (*cursor == '\0') continue;

        if (strlen(cursor) >= RELISH_TEXT_MAX) {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: line is too long (max %d)", load_label(depth, file_name, line_no),
                RELISH_TEXT_MAX - 1
            );
            result = -1;
            break;
        }

        char keyword[RELISH_KEYWORD_MAX];
        char *rest = read_token(cursor, keyword, sizeof(keyword));

        if (strcasecmp(keyword, "INCLUDE") == 0) {
            skip_ws(&rest);

            if (strcspn(rest, " \t") >= MACRO_NAME_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: INCLUDE file name is too long",
                    load_label(depth, file_name, line_no)
                );
                result = -1;
                break;
            }

            char include_name[MACRO_NAME_MAX];
            char *include_rest = read_token(rest, include_name, sizeof(include_name));
            skip_ws(&include_rest);

            if (!include_name[0]) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: INCLUDE requires a file name",
                    load_label(depth, file_name, line_no)
                );
                result = -1;
                break;
            }
            if (*include_rest != '\0') {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: INCLUDE takes one file name",
                    load_label(depth, file_name, line_no)
                );
                result = -1;
                break;
            }
            if (strchr(include_name, '/') || strchr(include_name, '\\')) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: INCLUDE cannot use a path, only a file name",
                    load_label(depth, file_name, line_no)
                );
                result = -1;
                break;
            }

            if (!strchr(include_name, '.')) {
                const size_t used = strlen(include_name);
                snprintf(include_name + used, sizeof(include_name) - used, ".rli");
            }

            if (!source_name_is_valid(include_name, depth + 1)) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: INCLUDE must name a same-directory .rli file",
                    load_label(depth, file_name, line_no)
                );
                result = -1;
                break;
            }

            char from_here[96];
            snprintf(from_here, sizeof(from_here), "%s", load_label(depth, file_name, line_no));

            result = load_source(prog, dir_fd, include_name, depth + 1, from_here, error);
            if (result != 0) break;
            continue;
        }

        if (prog->count >= RELISH_LINE_MAX) {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: script is too long (max %d lines)",
                load_label(depth, file_name, line_no), RELISH_LINE_MAX
            );
            result = -1;
            break;
        }

        snprintf(prog->lines[prog->count].text, RELISH_TEXT_MAX, "%s", cursor);
        prog->lines[prog->count].line_no = line_no;
        prog->lines[prog->count].source = source;
        prog->count++;
    }

    fclose(f);
    return result;
}

static int lookup_label(const struct relish_label *labels, const int label_count, const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcasecmp(labels[i].name, name) == 0) return labels[i].index;
    }
    return -1;
}

static int lookup_break_target(const struct relish_break_ref *breaks, const int break_count, const int line_no) {
    for (int i = 0; i < break_count; i++) {
        if (breaks[i].line_no == line_no) return breaks[i].target_index;
    }
    return -1;
}

static int resolve_number(const struct relish_compile *ctx, const char *token, long *out) {
    for (int i = 0; i < ctx->define_count; i++) {
        if (strcasecmp(ctx->defines[i].name, token) == 0) {
            if (ctx->defines[i].value % MACRO_VALUE_SCALE != 0) return -1;
            *out = ctx->defines[i].value / MACRO_VALUE_SCALE;
            return 0;
        }
    }

    char *end = NULL;
    const long value = strtol(token, &end, 10);
    if (end == token || *end != '\0') return -1;

    *out = value;
    return 0;
}

static int resolve_fixed(const struct relish_compile *ctx, const char *token, int32_t *out) {
    for (int i = 0; i < ctx->define_count; i++) {
        if (strcasecmp(ctx->defines[i].name, token) == 0) {
            *out = ctx->defines[i].value;
            return 0;
        }
    }

    const char *cursor = token;
    int sign = 1;
    if (*cursor == '-' || *cursor == '+') {
        if (*cursor == '-') sign = -1;
        cursor++;
    }
    if (!isdigit((unsigned char) *cursor)) return -1;

    int64_t whole = 0;
    while (isdigit((unsigned char) *cursor)) {
        whole = whole * 10 + (*cursor++ - '0');
        if (whole > RELISH_VALUE_MAX) return -1;
    }

    int32_t fraction = 0;
    int places = 0;
    if (*cursor == '.') {
        cursor++;
        if (!isdigit((unsigned char) *cursor)) return -1;
        while (isdigit((unsigned char) *cursor)) {
            if (places >= 3) return -1;
            fraction = fraction * 10 + (*cursor++ - '0');
            places++;
        }
    }
    if (*cursor != '\0') return -1;
    while (places++ < 3)
        fraction *= 10;

    const int64_t value = sign * (whole * MACRO_VALUE_SCALE + fraction);
    if (value < -MACRO_VALUE_LIMIT || value > MACRO_VALUE_LIMIT) return -1;
    *out = (int32_t) value;
    return 0;
}

static int resolve_var(struct relish_compile *ctx, const char *name) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcasecmp(ctx->vars[i], name) == 0) return i;
    }
    if (ctx->var_count >= MACRO_VAR_MAX) return -1;

    snprintf(ctx->vars[ctx->var_count], RELISH_LABEL_NAME_MAX, "%s", name);
    return ctx->var_count++;
}

static int register_break(struct relish_compile *ctx, const int index, const int loop_depth, char *error) {
    if (loop_depth <= 0) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: BREAK outside of a LOOP", line_label(&ctx->prog, index));
        return -1;
    }
    if (ctx->break_count >= RELISH_BREAK_MAX) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: too many BREAK statements", line_label(&ctx->prog, index));
        return -1;
    }

    ctx->breaks[ctx->break_count].line_no = index;
    ctx->breaks[ctx->break_count].owning_depth = loop_depth;
    ctx->breaks[ctx->break_count].target_index = -1;
    ctx->break_count++;
    return 0;
}

static void scan_if_line(const char *cursor, int *has_else, int *has_break) {
    char temp[RELISH_TEXT_MAX];
    snprintf(temp, sizeof(temp), "%s", cursor);

    *has_else = 0;
    *has_break = 0;

    int expecting_action = 0;
    char *saveptr = NULL;

    for (const char *tok = strtok_r(temp, " \t", &saveptr); tok; tok = strtok_r(NULL, " \t", &saveptr)) {
        if (strcasecmp(tok, "ELSE") == 0) {
            *has_else = 1;
            expecting_action = 1;
            continue;
        }
        if (strcasecmp(tok, "THEN") == 0) {
            expecting_action = 1;
            continue;
        }
        if (expecting_action) {
            if (strcasecmp(tok, "BREAK") == 0) *has_break = 1;
            expecting_action = 0;
        }
    }
}

static int set_define(
    struct relish_compile *ctx, char *cursor, const int index, const int redefine, char *error
) {
    const char *directive = redefine ? "REDEFINE" : "DEFINE";
    char name[RELISH_LABEL_NAME_MAX];
    cursor = read_token(cursor, name, sizeof(name));
    skip_ws(&cursor);

    if (!name[0]) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: %s requires a name", line_label(&ctx->prog, index), directive);
        return -1;
    }
    if (isdigit((unsigned char) name[0]) || name[0] == '-' || button_bit_for_token(name) >= 0) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: '%s' is not a usable %s name", line_label(&ctx->prog, index), name,
            directive
        );
        return -1;
    }

    int existing = -1;
    for (int i = 0; i < ctx->define_count; i++) {
        if (strcasecmp(ctx->defines[i].name, name) == 0) {
            existing = i;
            break;
        }
    }

    if (!redefine && existing >= 0) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: '%s' is already defined", line_label(&ctx->prog, index), name);
        return -1;
    }
    if (redefine && existing < 0) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: '%s' has not been defined", line_label(&ctx->prog, index), name);
        return -1;
    }
    if (!redefine && ctx->define_count >= RELISH_DEFINE_MAX) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: too many DEFINE values (max %d)", line_label(&ctx->prog, index),
            RELISH_DEFINE_MAX
        );
        return -1;
    }

    char value_token[RELISH_LABEL_NAME_MAX];
    read_token(cursor, value_token, sizeof(value_token));

    int32_t value;
    if (!value_token[0] || resolve_fixed(ctx, value_token, &value) != 0) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: %s requires a number", line_label(&ctx->prog, index), directive);
        return -1;
    }

    if (redefine) {
        ctx->defines[existing].value = value;
        return 0;
    }

    snprintf(ctx->defines[ctx->define_count].name, RELISH_LABEL_NAME_MAX, "%s", name);
    ctx->defines[ctx->define_count].value = value;
    ctx->define_count++;
    return 0;
}

static int first_pass(struct relish_compile *ctx, char *error) {
    int step_index = 0;

    int loop_start_lines[RELISH_LOOP_NEST_MAX];
    int loop_ordinals[RELISH_LOOP_NEST_MAX];
    int loop_depth = 0;
    int next_loop_ordinal = 0;

    for (int index = 0; index < ctx->prog.count; index++) {
        char line[RELISH_TEXT_MAX];
        snprintf(line, sizeof(line), "%s", ctx->prog.lines[index].text);

        char *cursor = line;
        char keyword[RELISH_KEYWORD_MAX];
        cursor = read_token(cursor, keyword, sizeof(keyword));
        skip_ws(&cursor);

        if (strcasecmp(keyword, "REM") == 0 || strcasecmp(keyword, "NAME") == 0) continue;

        if (strcasecmp(keyword, "DEFINE") == 0 || strcasecmp(keyword, "REDEFINE") == 0) {
            if (set_define(ctx, cursor, index, strcasecmp(keyword, "REDEFINE") == 0, error) != 0) return -1;
        } else if (strcasecmp(keyword, "LABEL") == 0) {
            if (*cursor == '\0') {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: LABEL requires a name", line_label(&ctx->prog, index));
                return -1;
            }

            char label_name[RELISH_LABEL_NAME_MAX];
            read_token(cursor, label_name, sizeof(label_name));

            for (int i = 0; i < ctx->label_count; i++) {
                if (strcasecmp(ctx->labels[i].name, label_name) == 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: label '%s' already defined", line_label(&ctx->prog, index),
                        label_name
                    );
                    return -1;
                }
            }
            if (ctx->label_count >= RELISH_LABEL_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: too many labels (max %d)", line_label(&ctx->prog, index),
                    RELISH_LABEL_MAX
                );
                return -1;
            }

            snprintf(ctx->labels[ctx->label_count].name, RELISH_LABEL_NAME_MAX, "%s", label_name);
            ctx->labels[ctx->label_count].index = step_index;
            ctx->label_count++;
        } else if (strcasecmp(keyword, "BUTTON") == 0 || strcasecmp(keyword, "GOTO") == 0
                   || strcasecmp(keyword, "PAUSE") == 0 || strcasecmp(keyword, "STICK") == 0
                   || strcasecmp(keyword, "SET") == 0 || strcasecmp(keyword, "INC") == 0
                   || strcasecmp(keyword, "DEC") == 0 || strcasecmp(keyword, "ADD") == 0
                   || strcasecmp(keyword, "SUB") == 0 || strcasecmp(keyword, "MUL") == 0
                   || strcasecmp(keyword, "DIV") == 0 || strcasecmp(keyword, "MOD") == 0
                   || strcasecmp(keyword, "SIN") == 0 || strcasecmp(keyword, "COS") == 0
                   || strcasecmp(keyword, "TAN") == 0 || strcasecmp(keyword, "FLR") == 0
                   || strcasecmp(keyword, "TOP") == 0
                   || strcasecmp(keyword, "CALL") == 0 || strcasecmp(keyword, "RETURN") == 0
                   || strcasecmp(keyword, "STOP") == 0) {
            step_index++;
        } else if (strcasecmp(keyword, "IF") == 0) {
            int has_else = 0;
            int has_break = 0;
            scan_if_line(cursor, &has_else, &has_break);

            if (has_break && register_break(ctx, index, loop_depth, error) != 0) return -1;

            step_index++;
            if (has_else) step_index++;
        } else if (strcasecmp(keyword, "LOOP") == 0) {
            if (loop_depth >= RELISH_LOOP_NEST_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: LOOP nesting is too deep (max %d)", line_label(&ctx->prog, index),
                    RELISH_LOOP_NEST_MAX
                );
                return -1;
            }
            if (next_loop_ordinal >= RELISH_LOOP_BLOCK_MAX) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: too many LOOP blocks", line_label(&ctx->prog, index));
                return -1;
            }

            loop_start_lines[loop_depth] = index;
            loop_ordinals[loop_depth] = next_loop_ordinal;
            next_loop_ordinal++;
            loop_depth++;
        } else if (strcasecmp(keyword, "ENDLOOP") == 0) {
            if (loop_depth <= 0) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: ENDLOOP without a matching LOOP", line_label(&ctx->prog, index)
                );
                return -1;
            }

            const int ordinal = loop_ordinals[loop_depth - 1];
            ctx->loop_instr_index[ordinal] = step_index;
            step_index++;

            for (int i = 0; i < ctx->break_count; i++) {
                if (ctx->breaks[i].target_index == -1 && ctx->breaks[i].owning_depth == loop_depth) {
                    ctx->breaks[i].target_index = step_index;
                }
            }

            loop_depth--;
        } else if (strcasecmp(keyword, "BREAK") == 0) {
            if (register_break(ctx, index, loop_depth, error) != 0) return -1;
            step_index++;
        } else {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: unrecognised keyword '%s'", line_label(&ctx->prog, index), keyword
            );
            return -1;
        }
    }

    if (loop_depth > 0) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: LOOP without a matching ENDLOOP",
            line_label(&ctx->prog, loop_start_lines[loop_depth - 1])
        );
        return -1;
    }

    ctx->step_count = step_index;
    return 0;
}

struct loop_ctx {
    int body_start;
    int count;
    int ordinal;
};

static int read_random_range(
    const struct relish_compile *ctx, char **saveptr, const char *what, const int index, long *low, long *high,
    char *error
) {
    char *low_token = strtok_r(NULL, " \t", saveptr);
    char *high_token = low_token ? strtok_r(NULL, " \t", saveptr) : NULL;

    if (!low_token || !high_token) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: %s RANDOM requires a smallest and a largest value",
            line_label(&ctx->prog, index), what
        );
        return -1;
    }

    if (resolve_number(ctx, low_token, low) != 0 || resolve_number(ctx, high_token, high) != 0 || *low < 0
        || *high > RELISH_VALUE_MAX || *high < *low) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: invalid %s RANDOM range", line_label(&ctx->prog, index), what);
        return -1;
    }

    return 0;
}

static int parse_modifier(
    const struct relish_compile *ctx, struct macro_step *step, const char *modifier, char **saveptr, const int index,
    char *error
) {
    char *token = strtok_r(NULL, " \t", saveptr);
    if (!token) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: %s requires a value", line_label(&ctx->prog, index), modifier);
        return -1;
    }

    const int is_wait = strcasecmp(modifier, "WAIT") == 0;
    const int is_hold = strcasecmp(modifier, "HOLD") == 0;

    if (strcasecmp(token, "RANDOM") == 0) {
        if (!is_wait && !is_hold) {
            snprintf(error, RELISH_ERROR_LENGTH, "%s: REPEAT cannot be RANDOM", line_label(&ctx->prog, index));
            return -1;
        }

        long low;
        long high;
        if (read_random_range(ctx, saveptr, modifier, index, &low, &high, error) != 0) return -1;

        if (is_wait) {
            step->wait_ms = (int) low;
            step->wait_rand_ms = (int) high;
        } else {
            step->hold_ms = (int) low;
            step->hold_rand_ms = (int) high;
        }

        return 0;
    }

    long value;
    if (resolve_number(ctx, token, &value) != 0 || value < 0 || value > RELISH_VALUE_MAX) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: invalid number '%s' after %s", line_label(&ctx->prog, index), token,
            modifier
        );
        return -1;
    }

    if (is_wait) {
        step->wait_ms = (int) value;
    } else if (is_hold) {
        step->hold_ms = (int) value;
    } else {
        step->repeat = value < 1 ? 1 : (int) value;
    }

    return 0;
}

static int is_modifier_word(const char *token) {
    return strcasecmp(token, "WAIT") == 0 || strcasecmp(token, "HOLD") == 0 || strcasecmp(token, "REPEAT") == 0;
}

static int parse_if_action(
    const struct relish_compile *ctx, char **saveptr, const int index, const char *clause, int *out_target, char *error
) {
    char *action = strtok_r(NULL, " \t", saveptr);
    if (!action) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: %s requires an action", line_label(&ctx->prog, index), clause);
        return -1;
    }

    if (strcasecmp(action, "BREAK") == 0) {
        *out_target = lookup_break_target(ctx->breaks, ctx->break_count, index);
        if (*out_target < 0) {
            snprintf(error, RELISH_ERROR_LENGTH, "%s: BREAK outside of a LOOP", line_label(&ctx->prog, index));
            return -1;
        }
        return 0;
    }

    if (strcasecmp(action, "STOP") == 0) {
        *out_target = ctx->step_count;
        return 0;
    }

    if (strcasecmp(action, "GOTO") != 0) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: %s must be followed by GOTO, BREAK or STOP",
            line_label(&ctx->prog, index), clause
        );
        return -1;
    }

    char *label_token = strtok_r(NULL, " \t", saveptr);
    if (!label_token) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: GOTO requires a label", line_label(&ctx->prog, index));
        return -1;
    }

    const int target = lookup_label(ctx->labels, ctx->label_count, label_token);
    if (target < 0) {
        snprintf(error, RELISH_ERROR_LENGTH, "%s: undefined label '%s'", line_label(&ctx->prog, index), label_token);
        return -1;
    }

    *out_target = target;
    return 0;
}

static int parse_variable_operand(
    struct relish_compile *ctx, char **saveptr, const int index, const char *keyword, const int optional,
    int *is_variable, int *variable_index, int32_t *value, char *error
) {
    char *token = strtok_r(NULL, " \t", saveptr);
    if (!token) {
        if (optional == 1) {
            *value = MACRO_VALUE_SCALE;
            return 0;
        }
        if (optional == 2) {
            *is_variable = 1;
            return 0;
        }
        snprintf(error, RELISH_ERROR_LENGTH, "%s: %s requires a value", line_label(&ctx->prog, index), keyword);
        return -1;
    }

    if (strcasecmp(token, "VAR") == 0) {
        char *name = strtok_r(NULL, " \t", saveptr);
        if (!name) {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: %s VAR requires a variable name", line_label(&ctx->prog, index),
                keyword
            );
            return -1;
        }

        const int slot = resolve_var(ctx, name);
        if (slot < 0) {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: too many variables (max %d)", line_label(&ctx->prog, index),
                MACRO_VAR_MAX
            );
            return -1;
        }
        *is_variable = 1;
        *variable_index = slot;
        return 0;
    }

    if (resolve_fixed(ctx, token, value) != 0) {
        snprintf(
            error, RELISH_ERROR_LENGTH, "%s: invalid %s value '%s'", line_label(&ctx->prog, index), keyword, token
        );
        return -1;
    }
    return 0;
}

static int emit_steps(
    struct relish_compile *ctx, struct macro_step *steps, int *step_count, char *name_out, int *name_set, char *error
) {
    *step_count = 0;

    struct loop_ctx loop_stack[RELISH_LOOP_NEST_MAX];
    int loop_depth = 0;
    int next_loop_ordinal = 0;

    for (int index = 0; index < ctx->prog.count; index++) {
        char line[RELISH_TEXT_MAX];
        snprintf(line, sizeof(line), "%s", ctx->prog.lines[index].text);

        char *cursor = line;
        char keyword[RELISH_KEYWORD_MAX];
        cursor = read_token(cursor, keyword, sizeof(keyword));
        skip_ws(&cursor);

        if (strcasecmp(keyword, "REM") == 0 || strcasecmp(keyword, "LABEL") == 0
            || strcasecmp(keyword, "DEFINE") == 0 || strcasecmp(keyword, "REDEFINE") == 0)
            continue;

        if (strcasecmp(keyword, "NAME") == 0) {
            if (*name_set) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: NAME can only be set once", line_label(&ctx->prog, index));
                return -1;
            }
            if (*cursor == '\0') {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: NAME requires a value", line_label(&ctx->prog, index));
                return -1;
            }

            snprintf(name_out, RELISH_DEF_LENGTH, "%s", cursor);
            *name_set = 1;
        } else if (strcasecmp(keyword, "BUTTON") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_button;
            step->wait_ms = MACRO_WAIT_MS_DEFAULT;
            step->hold_ms = MACRO_HOLD_MS_DEFAULT;
            step->repeat = MACRO_REPEAT_DEFAULT;

            int saw_button = 0;
            char *saveptr = NULL;
            char *token = strtok_r(cursor, " \t", &saveptr);
            while (token) {
                if (is_modifier_word(token)) {
                    char modifier[RELISH_KEYWORD_MAX];
                    snprintf(modifier, sizeof(modifier), "%s", token);

                    if (parse_modifier(ctx, step, modifier, &saveptr, index, error) != 0) return -1;
                } else {
                    const int bit = button_bit_for_token(token);
                    if (bit < 0) {
                        snprintf(
                            error, RELISH_ERROR_LENGTH, "%s: unknown button '%s'", line_label(&ctx->prog, index), token
                        );
                        return -1;
                    }
                    step->target_mask |= 1 << bit;
                    saw_button = 1;
                }
                token = strtok_r(NULL, " \t", &saveptr);
            }

            if (!saw_button) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: BUTTON requires at least one button", line_label(&ctx->prog, index)
                );
                return -1;
            }

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "STICK") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_stick;
            step->wait_ms = MACRO_WAIT_MS_DEFAULT;
            step->hold_ms = MACRO_HOLD_MS_DEFAULT;
            step->repeat = MACRO_REPEAT_DEFAULT;

            char *saveptr = NULL;
            char *side = strtok_r(cursor, " \t", &saveptr);
            if (!side) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: STICK requires LEFT or RIGHT", line_label(&ctx->prog, index));
                return -1;
            }

            if (strcasecmp(side, "LEFT") == 0) {
                step->stick_index = 0;
            } else if (strcasecmp(side, "RIGHT") == 0) {
                step->stick_index = 1;
            } else {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: STICK side must be LEFT or RIGHT, not '%s'",
                    line_label(&ctx->prog, index), side
                );
                return -1;
            }

            long axis[2];
            for (int a = 0; a < 2; a++) {
                char *axis_token = strtok_r(NULL, " \t", &saveptr);
                if (!axis_token || resolve_number(ctx, axis_token, &axis[a]) != 0 || axis[a] < -RELISH_AXIS_MAX
                    || axis[a] > RELISH_AXIS_MAX) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: STICK requires an X and a Y between -%d and %d",
                        line_label(&ctx->prog, index), RELISH_AXIS_MAX, RELISH_AXIS_MAX
                    );
                    return -1;
                }
            }
            step->axis_x = (int) axis[0];
            step->axis_y = (int) axis[1];

            char *token = strtok_r(NULL, " \t", &saveptr);
            while (token) {
                if (!is_modifier_word(token)) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: unexpected '%s' after STICK", line_label(&ctx->prog, index),
                        token
                    );
                    return -1;
                }

                char modifier[RELISH_KEYWORD_MAX];
                snprintf(modifier, sizeof(modifier), "%s", token);

                if (parse_modifier(ctx, step, modifier, &saveptr, index, error) != 0) return -1;
                token = strtok_r(NULL, " \t", &saveptr);
            }

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "SET") == 0 || strcasecmp(keyword, "INC") == 0
                   || strcasecmp(keyword, "DEC") == 0 || strcasecmp(keyword, "ADD") == 0
                   || strcasecmp(keyword, "SUB") == 0 || strcasecmp(keyword, "MUL") == 0
                   || strcasecmp(keyword, "DIV") == 0 || strcasecmp(keyword, "MOD") == 0
                   || strcasecmp(keyword, "SIN") == 0 || strcasecmp(keyword, "COS") == 0
                   || strcasecmp(keyword, "TAN") == 0 || strcasecmp(keyword, "FLR") == 0
                   || strcasecmp(keyword, "TOP") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            char *saveptr = NULL;
            char *name_token = strtok_r(cursor, " \t", &saveptr);
            if (!name_token) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: %s requires a variable name", line_label(&ctx->prog, index),
                    keyword
                );
                return -1;
            }

            const int slot = resolve_var(ctx, name_token);
            if (slot < 0) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: too many variables (max %d)", line_label(&ctx->prog, index),
                    MACRO_VAR_MAX
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_setvar;
            step->var_index = slot;

            if (strcasecmp(keyword, "SET") == 0) step->var_op = var_op_set;
            else if (strcasecmp(keyword, "INC") == 0 || strcasecmp(keyword, "ADD") == 0)
                step->var_op = var_op_add;
            else if (strcasecmp(keyword, "DEC") == 0 || strcasecmp(keyword, "SUB") == 0)
                step->var_op = var_op_subtract;
            else if (strcasecmp(keyword, "MUL") == 0)
                step->var_op = var_op_multiply;
            else if (strcasecmp(keyword, "DIV") == 0)
                step->var_op = var_op_divide;
            else if (strcasecmp(keyword, "MOD") == 0)
                step->var_op = var_op_modulo;
            else if (strcasecmp(keyword, "SIN") == 0)
                step->var_op = var_op_sine;
            else if (strcasecmp(keyword, "COS") == 0)
                step->var_op = var_op_cosine;
            else if (strcasecmp(keyword, "TAN") == 0)
                step->var_op = var_op_tangent;
            else if (strcasecmp(keyword, "FLR") == 0)
                step->var_op = var_op_floor;
            else
                step->var_op = var_op_ceiling;

            const int unary = step->var_op >= var_op_sine;
            const int optional = strcasecmp(keyword, "INC") == 0 || strcasecmp(keyword, "DEC") == 0 ? 1
                                                                                                      : unary ? 2 : 0;
            if (unary) step->var_rhs_index = slot;
            if (parse_variable_operand(
                    ctx, &saveptr, index, keyword, optional, &step->var_rhs_is_var, &step->var_rhs_index,
                    &step->var_value, error
                )
                != 0)
                return -1;

            if ((step->var_op == var_op_divide || step->var_op == var_op_modulo) && !step->var_rhs_is_var
                && step->var_value == 0) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: %s cannot use zero", line_label(&ctx->prog, index), keyword);
                return -1;
            }

            char *extra = strtok_r(NULL, " \t", &saveptr);
            if (extra) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: unexpected '%s' after %s", line_label(&ctx->prog, index), extra,
                    keyword
                );
                return -1;
            }

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "PAUSE") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_button;
            step->target_mask = 0;
            step->wait_ms = MACRO_WAIT_MS_DEFAULT;
            step->repeat = MACRO_REPEAT_DEFAULT;

            char *saveptr = NULL;
            char *ms_token = strtok_r(cursor, " \t", &saveptr);
            if (!ms_token) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: PAUSE requires a millisecond value", line_label(&ctx->prog, index)
                );
                return -1;
            }

            if (strcasecmp(ms_token, "RANDOM") == 0) {
                long low;
                long high;
                if (read_random_range(ctx, &saveptr, "PAUSE", index, &low, &high, error) != 0) return -1;

                step->hold_ms = (int) low;
                step->hold_rand_ms = (int) high;
            } else {
                long ms;
                if (resolve_number(ctx, ms_token, &ms) != 0 || ms < 0 || ms > RELISH_VALUE_MAX) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: invalid PAUSE value '%s'", line_label(&ctx->prog, index),
                        ms_token
                    );
                    return -1;
                }
                step->hold_ms = (int) ms;
            }

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "STOP") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_goto;
            step->jump_target = ctx->step_count;

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "RETURN") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }
            if (*cursor != '\0') {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: RETURN takes no value", line_label(&ctx->prog, index));
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_return;

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "GOTO") == 0 || strcasecmp(keyword, "CALL") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            char *saveptr = NULL;
            char *label_token = strtok_r(cursor, " \t", &saveptr);
            if (!label_token) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: %s requires a label", line_label(&ctx->prog, index), keyword
                );
                return -1;
            }

            char *extra = strtok_r(NULL, " \t", &saveptr);
            if (extra) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: unexpected '%s' after %s", line_label(&ctx->prog, index), extra,
                    keyword
                );
                return -1;
            }

            const int target = lookup_label(ctx->labels, ctx->label_count, label_token);
            if (target < 0) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: undefined label '%s'", line_label(&ctx->prog, index), label_token
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = strcasecmp(keyword, "CALL") == 0 ? macro_step_call : macro_step_goto;
            step->jump_target = target;

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "LOOP") == 0) {
            char *saveptr = NULL;
            char *count_token = strtok_r(cursor, " \t", &saveptr);
            if (!count_token) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: LOOP requires a count", line_label(&ctx->prog, index));
                return -1;
            }

            long count;
            if (resolve_number(ctx, count_token, &count) != 0 || count < 1 || count > RELISH_VALUE_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: invalid LOOP count '%s'", line_label(&ctx->prog, index),
                    count_token
                );
                return -1;
            }

            loop_stack[loop_depth].body_start = *step_count;
            loop_stack[loop_depth].count = (int) count;
            loop_stack[loop_depth].ordinal = next_loop_ordinal;
            next_loop_ordinal++;
            loop_depth++;
        } else if (strcasecmp(keyword, "ENDLOOP") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            if (loop_depth <= 0) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: ENDLOOP without a matching LOOP", line_label(&ctx->prog, index)
                );
                return -1;
            }
            loop_depth--;
            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_loop;
            step->jump_target = loop_stack[loop_depth].body_start;
            step->loop_count = loop_stack[loop_depth].count - 1;

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "BREAK") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            const int target = lookup_break_target(ctx->breaks, ctx->break_count, index);
            if (target < 0) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: BREAK outside of a LOOP", line_label(&ctx->prog, index));
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_goto;
            step->jump_target = target;

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else if (strcasecmp(keyword, "IF") == 0) {
            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            struct macro_step *step = &steps[*step_count];
            memset(step, 0, sizeof(*step));
            step->kind = macro_step_if;

            char *saveptr = NULL;
            char *token = strtok_r(cursor, " \t", &saveptr);
            if (!token) {
                snprintf(error, RELISH_ERROR_LENGTH, "%s: IF requires a condition", line_label(&ctx->prog, index));
                return -1;
            }

            if (strcasecmp(token, "NOT") == 0) {
                step->if_negate = 1;
                token = strtok_r(NULL, " \t", &saveptr);
                if (!token) {
                    snprintf(error, RELISH_ERROR_LENGTH, "%s: NOT requires a condition", line_label(&ctx->prog, index));
                    return -1;
                }
            }

            if (strcasecmp(token, "COUNT") == 0) {
                step->if_test = if_test_count_compare;

                char *op_token = strtok_r(NULL, " \t", &saveptr);
                if (!op_token) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: COUNT requires a comparison operator",
                        line_label(&ctx->prog, index)
                    );
                    return -1;
                }
                if (op_from_word(op_token, &step->if_op) != 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: unknown comparison operator '%s'",
                        line_label(&ctx->prog, index), op_token
                    );
                    return -1;
                }

                char *n_token = strtok_r(NULL, " \t", &saveptr);
                long n;
                if (!n_token || resolve_number(ctx, n_token, &n) != 0 || n < 1 || n > RELISH_VALUE_MAX) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: COUNT comparison requires a number from 1 upwards",
                        line_label(&ctx->prog, index)
                    );
                    return -1;
                }
                step->loop_count = (int) n;

                if (loop_depth <= 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: COUNT used outside of a LOOP", line_label(&ctx->prog, index)
                    );
                    return -1;
                }
                step->if_loop_ref = ctx->loop_instr_index[loop_stack[loop_depth - 1].ordinal];
            } else if (strcasecmp(token, "RANDOM") == 0) {
                step->if_test = if_test_random;

                char *pct_token = strtok_r(NULL, " \t", &saveptr);
                long pct;
                if (!pct_token || resolve_number(ctx, pct_token, &pct) != 0 || pct < 0 || pct > 100) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: RANDOM requires a percentage from 0 to 100",
                        line_label(&ctx->prog, index)
                    );
                    return -1;
                }
                step->loop_count = (int) pct;
            } else if (strcasecmp(token, "VAR") == 0) {
                step->if_test = if_test_var_compare;

                char *name_token = strtok_r(NULL, " \t", &saveptr);
                if (!name_token) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: VAR requires a variable name", line_label(&ctx->prog, index)
                    );
                    return -1;
                }

                const int slot = resolve_var(ctx, name_token);
                if (slot < 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: too many variables (max %d)", line_label(&ctx->prog, index),
                        MACRO_VAR_MAX
                    );
                    return -1;
                }
                step->var_index = slot;

                char *op_token = strtok_r(NULL, " \t", &saveptr);
                if (!op_token) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: VAR requires a comparison operator",
                        line_label(&ctx->prog, index)
                    );
                    return -1;
                }
                if (op_from_word(op_token, &step->if_op) != 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: unknown comparison operator '%s'",
                        line_label(&ctx->prog, index), op_token
                    );
                    return -1;
                }

                char *rhs_token = strtok_r(NULL, " \t", &saveptr);
                if (!rhs_token) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: VAR comparison requires a number or VAR name",
                        line_label(&ctx->prog, index)
                    );
                    return -1;
                }

                if (strcasecmp(rhs_token, "VAR") == 0) {
                    char *rhs_name = strtok_r(NULL, " \t", &saveptr);
                    if (!rhs_name) {
                        snprintf(
                            error, RELISH_ERROR_LENGTH, "%s: comparison VAR requires a variable name",
                            line_label(&ctx->prog, index)
                        );
                        return -1;
                    }

                    const int rhs_slot = resolve_var(ctx, rhs_name);
                    if (rhs_slot < 0) {
                        snprintf(
                            error, RELISH_ERROR_LENGTH, "%s: too many variables (max %d)",
                            line_label(&ctx->prog, index), MACRO_VAR_MAX
                        );
                        return -1;
                    }
                    step->if_rhs_is_var = 1;
                    step->if_rhs_var_index = rhs_slot;
                } else if (resolve_fixed(ctx, rhs_token, &step->var_value) != 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: VAR comparison requires a valid number or VAR name",
                        line_label(&ctx->prog, index)
                    );
                    return -1;
                }
            } else {
                step->if_test = if_test_button_held;

                const int bit = button_bit_for_token(token);
                if (bit < 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: unknown button '%s'", line_label(&ctx->prog, index), token
                    );
                    return -1;
                }
                step->target_mask = 1 << bit;

                char *held_token = strtok_r(NULL, " \t", &saveptr);
                if (!held_token || strcasecmp(held_token, "HELD") != 0) {
                    snprintf(
                        error, RELISH_ERROR_LENGTH, "%s: expected HELD after button", line_label(&ctx->prog, index)
                    );
                    return -1;
                }
            }

            char *then_token = strtok_r(NULL, " \t", &saveptr);
            if (!then_token || strcasecmp(then_token, "THEN") != 0) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: IF condition must be followed by THEN",
                    line_label(&ctx->prog, index)
                );
                return -1;
            }

            int then_target = 0;
            if (parse_if_action(ctx, &saveptr, index, "THEN", &then_target, error) != 0) return -1;
            step->jump_target = then_target;

            ctx->step_line[*step_count] = index;
            (*step_count)++;

            char *else_token = strtok_r(NULL, " \t", &saveptr);
            if (!else_token) continue;

            if (strcasecmp(else_token, "ELSE") != 0) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: unexpected '%s' after the THEN action",
                    line_label(&ctx->prog, index), else_token
                );
                return -1;
            }

            if (*step_count >= MACRO_STEP_MAX) {
                snprintf(
                    error, RELISH_ERROR_LENGTH, "%s: macro exceeds %d steps", line_label(&ctx->prog, index),
                    MACRO_STEP_MAX
                );
                return -1;
            }

            int else_target = 0;
            if (parse_if_action(ctx, &saveptr, index, "ELSE", &else_target, error) != 0) return -1;

            struct macro_step *otherwise = &steps[*step_count];
            memset(otherwise, 0, sizeof(*otherwise));
            otherwise->kind = macro_step_goto;
            otherwise->jump_target = else_target;

            ctx->step_line[*step_count] = index;
            (*step_count)++;
        } else {
            snprintf(
                error, RELISH_ERROR_LENGTH, "%s: unrecognised keyword '%s'", line_label(&ctx->prog, index), keyword
            );
            return -1;
        }
    }

    return 0;
}

struct explore_item {
    int step;
    int trail_len;
};

struct explore_trail {
    int slot;
    int value;
};

static int explore_jump_safety(const struct macro_step *steps, const int step_count, const int start, int *budget) {
    int loop_progress[MACRO_STEP_MAX] = {0};

    struct explore_item pending[RELISH_SAFETY_BUDGET + 1];
    int pending_count = 0;

    struct explore_trail trail[RELISH_SAFETY_BUDGET + 1];
    int trail_len = 0;

    pending[pending_count].step = start;
    pending[pending_count].trail_len = 0;
    pending_count++;

    while (pending_count > 0) {
        pending_count--;
        int cur = pending[pending_count].step;

        while (trail_len > pending[pending_count].trail_len) {
            trail_len--;
            loop_progress[trail[trail_len].slot] = trail[trail_len].value;
        }

        while (1) {
            if (cur >= step_count) break;
            if (cur < 0 || *budget <= 0) return 0;
            (*budget)--;

            const struct macro_step *step = &steps[cur];

            if (step->kind == macro_step_button || step->kind == macro_step_stick) break;

            if (step->kind == macro_step_setvar) {
                cur++;
                continue;
            }

            if (step->kind == macro_step_goto) {
                cur = step->jump_target;
                continue;
            }

            if (step->kind == macro_step_call) {
                if (pending_count >= RELISH_SAFETY_BUDGET) return 0;
                pending[pending_count].step = cur + 1;
                pending[pending_count].trail_len = trail_len;
                pending_count++;
                cur = step->jump_target;
                continue;
            }

            if (step->kind == macro_step_return) break;

            if (step->kind == macro_step_loop) {
                trail[trail_len].slot = cur;
                trail[trail_len].value = loop_progress[cur];
                trail_len++;

                if (loop_progress[cur] < step->loop_count) {
                    loop_progress[cur]++;
                    cur = step->jump_target;
                } else {
                    loop_progress[cur] = 0;
                    cur++;
                }
                continue;
            }

            if (pending_count >= RELISH_SAFETY_BUDGET) return 0;
            pending[pending_count].step = cur + 1;
            pending[pending_count].trail_len = trail_len;
            pending_count++;

            cur = step->jump_target;
        }
    }

    return 1;
}

static int validate_jump_safety(
    const struct relish_compile *ctx, const struct macro_step *steps, const int step_count, char *error
) {
    for (int start = 0; start < step_count; start++) {
        int budget = RELISH_SAFETY_BUDGET;

        if (!explore_jump_safety(steps, step_count, start, &budget)) {
            snprintf(
                error, RELISH_ERROR_LENGTH,
                "%s: this Goto/Call/Loop/If chain never reaches a Button/Stick step or the end of the macro (or is too "
                "complex to verify)",
                line_label(&ctx->prog, ctx->step_line[start])
            );
            return -1;
        }
    }

    return 0;
}

int relish_compile_file(const char *path, struct macro_entry *out_entry) {
    memset(out_entry, 0, sizeof(*out_entry));
    out_entry->is_relish = 1;
    out_entry->index = -1;
    snprintf(out_entry->path, sizeof(out_entry->path), "%s", path);
    fallback_name_from_path(path, out_entry->name);

    struct relish_compile *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        snprintf(out_entry->compile_error, sizeof(out_entry->compile_error), "Not enough memory to compile");
        return -1;
    }

    char dir[MACRO_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);

    char *slash = strrchr(dir, '/');
    if (slash) {
        if (slash == dir) slash[1] = '\0';
        else *slash = '\0';
    } else {
        snprintf(dir, sizeof(dir), ".");
    }

    const int dir_fd = open(dir, O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (dir_fd < 0) {
        snprintf(out_entry->compile_error, sizeof(out_entry->compile_error), "Could not open script directory");
        free(ctx);
        return -1;
    }

    const int load_result =
        load_source(&ctx->prog, dir_fd, basename_of(path), 0, "Script", out_entry->compile_error);
    close(dir_fd);
    if (load_result != 0) {
        free(ctx);
        return -1;
    }
    out_entry->created = ctx->prog.source_mtime;

    if (first_pass(ctx, out_entry->compile_error) != 0) {
        free(ctx);
        return -1;
    }

    int name_set = 0;
    const int emit_result =
        emit_steps(ctx, out_entry->steps, &out_entry->step_count, out_entry->name, &name_set, out_entry->compile_error);

    if (emit_result != 0) {
        free(ctx);
        return -1;
    }

    if (out_entry->step_count <= 0) {
        snprintf(
            out_entry->compile_error, sizeof(out_entry->compile_error),
            "Script has no executable instructions"
        );
        free(ctx);
        return -1;
    }

    if (validate_jump_safety(ctx, out_entry->steps, out_entry->step_count, out_entry->compile_error) != 0) {
        free(ctx);
        return -1;
    }

    free(ctx);

    out_entry->index = relish_registry_lookup(path);
    return 0;
}

void relish_registry_load(const char *macro_dir) {
    registry_count = 0;
    registry_dirty = 0;

    char registry_path[MACRO_PATH_MAX];
    snprintf(registry_path, sizeof(registry_path), "%s/%s", macro_dir, RELISH_REGISTRY_FILE);

    mini_t *ini = mini_try_load(registry_path);
    if (!ini) return;

    const int count = (int) mini_get_int(ini, RELISH_GROUP, "count", 0);
    for (int i = 0; i < count && registry_count < RELISH_REGISTRY_MAX; i++) {
        char file_key[24];
        char index_key[24];
        snprintf(file_key, sizeof(file_key), "entry%d_file", i);
        snprintf(index_key, sizeof(index_key), "entry%d_index", i);

        const char *file = get_ini_string(ini, RELISH_GROUP, file_key, "");
        if (!file[0]) continue;

        snprintf(registry[registry_count].file, sizeof(registry[registry_count].file), "%s", file);
        registry[registry_count].index = (int) mini_get_int(ini, RELISH_GROUP, index_key, -1);
        registry_count++;
    }

    mini_free(ini);
}

int relish_registry_lookup(const char *rls_path) {
    const char *base = basename_of(rls_path);
    for (int i = 0; i < registry_count; i++) {
        if (strcasecmp(registry[i].file, base) == 0) return registry[i].index;
    }
    return -1;
}

void relish_registry_record(const char *rls_path, const int index) {
    const char *base = basename_of(rls_path);

    for (int i = 0; i < registry_count; i++) {
        if (strcasecmp(registry[i].file, base) == 0) {
            if (registry[i].index != index) {
                registry[i].index = index;
                registry_dirty = 1;
            }
            return;
        }
    }

    if (registry_count >= RELISH_REGISTRY_MAX) return;

    snprintf(registry[registry_count].file, sizeof(registry[registry_count].file), "%s", base);
    registry[registry_count].index = index;
    registry_count++;
    registry_dirty = 1;
}

void relish_registry_finalise(const struct macro_entry *entries, const int count, const char *macro_dir) {
    for (int i = 0; i < registry_count;) {
        int found = 0;
        for (int e = 0; e < count; e++) {
            if (entries[e].is_relish && strcasecmp(basename_of(entries[e].path), registry[i].file) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            for (int j = i; j < registry_count - 1; j++)
                registry[j] = registry[j + 1];
            registry_count--;
            registry_dirty = 1;
        } else {
            i++;
        }
    }

    if (!registry_dirty) return;

    char registry_path[MACRO_PATH_MAX];
    snprintf(registry_path, sizeof(registry_path), "%s/%s", macro_dir, RELISH_REGISTRY_FILE);

    mini_t *ini = mini_create(registry_path);
    if (!ini) return;

    mini_set_int(ini, RELISH_GROUP, "count", registry_count);
    for (int i = 0; i < registry_count; i++) {
        char file_key[24];
        char index_key[24];
        snprintf(file_key, sizeof(file_key), "entry%d_file", i);
        snprintf(index_key, sizeof(index_key), "entry%d_index", i);

        mini_set_string(ini, RELISH_GROUP, file_key, registry[i].file);
        mini_set_int(ini, RELISH_GROUP, index_key, registry[i].index);
    }

    mini_save(ini, 0);
    mini_free(ini);
    registry_dirty = 0;
}
