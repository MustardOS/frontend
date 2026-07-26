#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
#define RELISH_SAFETY_BUDGET  256
#define RELISH_DEF_LENGTH     128

struct relish_label {
    char name[RELISH_LABEL_NAME_MAX];
    int index;
};

struct relish_break_ref {
    int line_no;
    int owning_depth;
    int target_index;
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
    size_t len = 0;
    while (cursor[len] && !isspace((unsigned char) cursor[len]) && len < out_len - 1) {
        out[len] = cursor[len];
        len++;
    }
    out[len] = '\0';
    return cursor + len;
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

static int register_break(
    struct relish_break_ref *breaks, int *break_count, const int line_no, const int loop_depth, char *error
) {
    if (loop_depth <= 0) {
        snprintf(error, RELISH_DEF_LENGTH, "Line %d: BREAK outside of a LOOP", line_no);
        return -1;
    }
    if (*break_count >= RELISH_BREAK_MAX) {
        snprintf(error, RELISH_DEF_LENGTH, "Line %d: too many BREAK statements", line_no);
        return -1;
    }

    breaks[*break_count].line_no = line_no;
    breaks[*break_count].owning_depth = loop_depth;
    breaks[*break_count].target_index = -1;
    (*break_count)++;
    return 0;
}

static int if_line_ends_in_break(const char *cursor) {
    char temp[MAX_BUFFER_SIZE];
    snprintf(temp, sizeof(temp), "%s", cursor);

    char *saveptr = NULL;
    const char *prev = NULL;
    const char *last = NULL;
    const char *tok = strtok_r(temp, " \t", &saveptr);
    while (tok) {
        prev = last;
        last = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    return last && prev && strcasecmp(last, "BREAK") == 0 && strcasecmp(prev, "THEN") == 0;
}

static int first_pass(
    FILE *f, struct relish_label *labels, int *label_count, int *loop_instr_index, struct relish_break_ref *breaks,
    int *break_count, char *error
) {
    rewind(f);
    char line[MAX_BUFFER_SIZE];
    int line_no = 0;
    int step_index = 0;

    int loop_start_lines[RELISH_LOOP_NEST_MAX];
    int loop_ordinals[RELISH_LOOP_NEST_MAX];
    int loop_depth = 0;
    int next_loop_ordinal = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        strip_crlf(line);

        char *cursor = line;
        skip_ws(&cursor);
        if (*cursor == '\0') continue;

        char keyword[RELISH_KEYWORD_MAX];
        cursor = read_token(cursor, keyword, sizeof(keyword));
        skip_ws(&cursor);

        if (strcasecmp(keyword, "REM") != 0 && strcasecmp(keyword, "NAME") != 0) {
            if (strcasecmp(keyword, "LABEL") == 0) {
                if (*cursor == '\0') {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: LABEL requires a name", line_no);
                    return -1;
                }

                char label_name[RELISH_LABEL_NAME_MAX];
                read_token(cursor, label_name, sizeof(label_name));

                for (int i = 0; i < *label_count; i++) {
                    if (strcasecmp(labels[i].name, label_name) == 0) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: label '%s' already defined", line_no, label_name);
                        return -1;
                    }
                }
                if (*label_count >= RELISH_LABEL_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: too many labels (max %d)", line_no, RELISH_LABEL_MAX);
                    return -1;
                }

                snprintf(labels[*label_count].name, sizeof(labels[*label_count].name), "%s", label_name);
                labels[*label_count].index = step_index;
                (*label_count)++;
            } else if (strcasecmp(keyword, "BUTTON") == 0 || strcasecmp(keyword, "GOTO") == 0
                       || strcasecmp(keyword, "PAUSE") == 0) {
                step_index++;
            } else if (strcasecmp(keyword, "IF") == 0) {
                if (if_line_ends_in_break(cursor)) {
                    if (register_break(breaks, break_count, line_no, loop_depth, error) != 0) return -1;
                }
                step_index++;
            } else if (strcasecmp(keyword, "LOOP") == 0) {
                if (loop_depth >= RELISH_LOOP_NEST_MAX) {
                    snprintf(
                        error, RELISH_DEF_LENGTH, "Line %d: LOOP nesting is too deep (max %d)", line_no,
                        RELISH_LOOP_NEST_MAX
                    );
                    return -1;
                }
                if (next_loop_ordinal >= RELISH_LOOP_BLOCK_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: too many LOOP blocks", line_no);
                    return -1;
                }

                loop_start_lines[loop_depth] = line_no;
                loop_ordinals[loop_depth] = next_loop_ordinal;
                next_loop_ordinal++;
                loop_depth++;
            } else if (strcasecmp(keyword, "ENDLOOP") == 0) {
                if (loop_depth <= 0) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: ENDLOOP without a matching LOOP", line_no);
                    return -1;
                }

                const int ordinal = loop_ordinals[loop_depth - 1];
                loop_instr_index[ordinal] = step_index;
                step_index++;

                for (int i = 0; i < *break_count; i++) {
                    if (breaks[i].target_index == -1 && breaks[i].owning_depth == loop_depth) {
                        breaks[i].target_index = step_index;
                    }
                }

                loop_depth--;
            } else if (strcasecmp(keyword, "BREAK") == 0) {
                if (register_break(breaks, break_count, line_no, loop_depth, error) != 0) return -1;
                step_index++;
            } else {
                snprintf(error, RELISH_DEF_LENGTH, "Line %d: unrecognised keyword '%s'", line_no, keyword);
                return -1;
            }
        }
    }

    if (loop_depth > 0) {
        snprintf(
            error, RELISH_DEF_LENGTH, "Line %d: LOOP without a matching ENDLOOP", loop_start_lines[loop_depth - 1]
        );
        return -1;
    }

    return 0;
}

struct loop_ctx {
    int body_start;
    int count;
    int ordinal;
};

static int emit_steps(
    FILE *f, const struct relish_label *labels, const int label_count, const int *loop_instr_index,
    const struct relish_break_ref *breaks, const int break_count, struct macro_step *steps, int *step_count,
    int *step_line, char *name_out, int *name_set, char *error
) {
    rewind(f);
    char line[MAX_BUFFER_SIZE];
    int line_no = 0;
    *step_count = 0;

    struct loop_ctx loop_stack[RELISH_LOOP_NEST_MAX];
    int loop_depth = 0;
    int next_loop_ordinal = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        strip_crlf(line);

        char *cursor = line;
        skip_ws(&cursor);
        if (*cursor == '\0') continue;

        char keyword[RELISH_KEYWORD_MAX];
        cursor = read_token(cursor, keyword, sizeof(keyword));
        skip_ws(&cursor);

        if (strcasecmp(keyword, "REM") != 0 && strcasecmp(keyword, "LABEL") != 0) {
            if (strcasecmp(keyword, "NAME") == 0) {
                if (*name_set) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: NAME can only be set once", line_no);
                    return -1;
                }
                if (*cursor == '\0') {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: NAME requires a value", line_no);
                    return -1;
                }
                snprintf(name_out, RELISH_DEF_LENGTH, "%s", cursor);
                *name_set = 1;
            } else if (strcasecmp(keyword, "BUTTON") == 0) {
                if (*step_count >= MACRO_STEP_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: macro exceeds %d steps", line_no, MACRO_STEP_MAX);
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
                    if (strcasecmp(token, "WAIT") == 0 || strcasecmp(token, "HOLD") == 0
                        || strcasecmp(token, "REPEAT") == 0) {
                        char modifier[RELISH_KEYWORD_MAX];
                        snprintf(modifier, sizeof(modifier), "%s", token);

                        token = strtok_r(NULL, " \t", &saveptr);
                        if (!token) {
                            snprintf(error, RELISH_DEF_LENGTH, "Line %d: %s requires a value", line_no, modifier);
                            return -1;
                        }

                        char *end = NULL;
                        const long value = strtol(token, &end, 10);
                        if (end == token || value < 0 || value > 65535) {
                            snprintf(
                                error, RELISH_DEF_LENGTH, "Line %d: invalid number '%s' after %s", line_no, token,
                                modifier
                            );
                            return -1;
                        }

                        if (strcasecmp(modifier, "WAIT") == 0)
                            step->wait_ms = (int) value;
                        else if (strcasecmp(modifier, "HOLD") == 0)
                            step->hold_ms = (int) value;
                        else
                            step->repeat = value < 1 ? 1 : (int) value;
                    } else {
                        const int bit = button_bit_for_token(token);
                        if (bit < 0) {
                            snprintf(error, RELISH_DEF_LENGTH, "Line %d: unknown button '%s'", line_no, token);
                            return -1;
                        }
                        step->target_mask |= 1 << bit;
                        saw_button = 1;
                    }
                    token = strtok_r(NULL, " \t", &saveptr);
                }

                if (!saw_button) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: BUTTON requires at least one button", line_no);
                    return -1;
                }

                step_line[*step_count] = line_no;
                (*step_count)++;
            } else if (strcasecmp(keyword, "PAUSE") == 0) {
                if (*step_count >= MACRO_STEP_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: macro exceeds %d steps", line_no, MACRO_STEP_MAX);
                    return -1;
                }

                char *saveptr = NULL;
                char *ms_token = strtok_r(cursor, " \t", &saveptr);
                if (!ms_token) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: PAUSE requires a millisecond value", line_no);
                    return -1;
                }

                char *end = NULL;
                const long ms = strtol(ms_token, &end, 10);
                if (end == ms_token || ms < 0 || ms > 65535) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: invalid PAUSE value '%s'", line_no, ms_token);
                    return -1;
                }

                struct macro_step *step = &steps[*step_count];
                memset(step, 0, sizeof(*step));
                step->kind = macro_step_button;
                step->target_mask = 0;
                step->wait_ms = MACRO_WAIT_MS_DEFAULT;
                step->hold_ms = (int) ms;
                step->repeat = MACRO_REPEAT_DEFAULT;

                step_line[*step_count] = line_no;
                (*step_count)++;
            } else if (strcasecmp(keyword, "GOTO") == 0) {
                if (*step_count >= MACRO_STEP_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: macro exceeds %d steps", line_no, MACRO_STEP_MAX);
                    return -1;
                }

                char *saveptr = NULL;
                char *label_token = strtok_r(cursor, " \t", &saveptr);
                if (!label_token) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: GOTO requires a label", line_no);
                    return -1;
                }

                const int target = lookup_label(labels, label_count, label_token);
                if (target < 0) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: undefined label '%s'", line_no, label_token);
                    return -1;
                }

                struct macro_step *step = &steps[*step_count];
                memset(step, 0, sizeof(*step));
                step->kind = macro_step_goto;
                step->jump_target = target;

                step_line[*step_count] = line_no;
                (*step_count)++;
            } else if (strcasecmp(keyword, "LOOP") == 0) {
                char *saveptr = NULL;
                char *count_token = strtok_r(cursor, " \t", &saveptr);
                if (!count_token) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: LOOP requires a count", line_no);
                    return -1;
                }

                char *end = NULL;
                const long count = strtol(count_token, &end, 10);
                if (end == count_token || count < 1 || count > 65535) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: invalid LOOP count '%s'", line_no, count_token);
                    return -1;
                }

                loop_stack[loop_depth].body_start = *step_count;
                loop_stack[loop_depth].count = (int) count;
                loop_stack[loop_depth].ordinal = next_loop_ordinal;
                next_loop_ordinal++;
                loop_depth++;
            } else if (strcasecmp(keyword, "ENDLOOP") == 0) {
                if (*step_count >= MACRO_STEP_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: macro exceeds %d steps", line_no, MACRO_STEP_MAX);
                    return -1;
                }

                loop_depth--;
                struct macro_step *step = &steps[*step_count];
                memset(step, 0, sizeof(*step));
                step->kind = macro_step_loop;
                step->jump_target = loop_stack[loop_depth].body_start;
                step->loop_count = loop_stack[loop_depth].count;

                step_line[*step_count] = line_no;
                (*step_count)++;
            } else if (strcasecmp(keyword, "BREAK") == 0) {
                if (*step_count >= MACRO_STEP_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: macro exceeds %d steps", line_no, MACRO_STEP_MAX);
                    return -1;
                }

                struct macro_step *step = &steps[*step_count];
                memset(step, 0, sizeof(*step));
                step->kind = macro_step_goto;
                step->jump_target = lookup_break_target(breaks, break_count, line_no);

                step_line[*step_count] = line_no;
                (*step_count)++;
            } else if (strcasecmp(keyword, "IF") == 0) {
                if (*step_count >= MACRO_STEP_MAX) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: macro exceeds %d steps", line_no, MACRO_STEP_MAX);
                    return -1;
                }

                struct macro_step *step = &steps[*step_count];
                memset(step, 0, sizeof(*step));
                step->kind = macro_step_if;

                char *saveptr = NULL;
                char *token1 = strtok_r(cursor, " \t", &saveptr);
                if (!token1) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: IF requires a condition", line_no);
                    return -1;
                }

                if (strcasecmp(token1, "COUNT") == 0) {
                    step->if_test = if_test_count_compare;

                    char *op_token = strtok_r(NULL, " \t", &saveptr);
                    if (!op_token) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: COUNT requires a comparison operator", line_no);
                        return -1;
                    }
                    if (op_from_word(op_token, &step->if_op) != 0) {
                        snprintf(
                            error, RELISH_DEF_LENGTH, "Line %d: unknown comparison operator '%s'", line_no, op_token
                        );
                        return -1;
                    }

                    char *n_token = strtok_r(NULL, " \t", &saveptr);
                    if (!n_token) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: COUNT comparison requires a number", line_no);
                        return -1;
                    }
                    char *end = NULL;
                    const long n = strtol(n_token, &end, 10);
                    if (end == n_token || n < 1 || n > 65535) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: invalid COUNT value '%s'", line_no, n_token);
                        return -1;
                    }
                    step->loop_count = (int) n;

                    if (loop_depth <= 0) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: COUNT used outside of a LOOP", line_no);
                        return -1;
                    }
                    step->if_loop_ref = loop_instr_index[loop_stack[loop_depth - 1].ordinal];
                } else {
                    step->if_test = if_test_button_held;

                    char *button_token = token1;
                    if (strcasecmp(token1, "NOT") == 0) {
                        step->if_negate = 1;
                        button_token = strtok_r(NULL, " \t", &saveptr);
                        if (!button_token) {
                            snprintf(error, RELISH_DEF_LENGTH, "Line %d: NOT requires a button", line_no);
                            return -1;
                        }
                    }

                    const int bit = button_bit_for_token(button_token);
                    if (bit < 0) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: unknown button '%s'", line_no, button_token);
                        return -1;
                    }
                    step->target_mask = 1 << bit;

                    char *held_token = strtok_r(NULL, " \t", &saveptr);
                    if (!held_token || strcasecmp(held_token, "HELD") != 0) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: expected HELD after button", line_no);
                        return -1;
                    }
                }

                char *then_token = strtok_r(NULL, " \t", &saveptr);
                if (!then_token || strcasecmp(then_token, "THEN") != 0) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: IF condition must be followed by THEN", line_no);
                    return -1;
                }

                char *action_token = strtok_r(NULL, " \t", &saveptr);
                if (!action_token) {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: THEN requires an action", line_no);
                    return -1;
                }

                if (strcasecmp(action_token, "BREAK") == 0) {
                    step->jump_target = lookup_break_target(breaks, break_count, line_no);
                } else if (strcasecmp(action_token, "GOTO") == 0) {
                    char *label_token = strtok_r(NULL, " \t", &saveptr);
                    if (!label_token) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: GOTO requires a label", line_no);
                        return -1;
                    }

                    const int target = lookup_label(labels, label_count, label_token);
                    if (target < 0) {
                        snprintf(error, RELISH_DEF_LENGTH, "Line %d: undefined label '%s'", line_no, label_token);
                        return -1;
                    }
                    step->jump_target = target;
                } else {
                    snprintf(error, RELISH_DEF_LENGTH, "Line %d: THEN must be followed by GOTO or BREAK", line_no);
                    return -1;
                }

                step_line[*step_count] = line_no;
                (*step_count)++;
            } else {
                snprintf(error, RELISH_DEF_LENGTH, "Line %d: unrecognised keyword '%s'", line_no, keyword);
                return -1;
            }
        }
    }

    return 0;
}

static int explore_jump_safety(
    const struct macro_step *steps, const int step_count, const int cur, int *loop_progress, int *budget
) {
    if (cur >= step_count) return 1;
    if (*budget <= 0) return 0;
    (*budget)--;

    const struct macro_step *step = &steps[cur];

    if (step->kind == macro_step_button) return 1;

    if (step->kind == macro_step_goto) {
        return explore_jump_safety(steps, step_count, step->jump_target, loop_progress, budget);
    }

    if (step->kind == macro_step_loop) {
        if (loop_progress[cur] < step->loop_count) {
            loop_progress[cur]++;
            return explore_jump_safety(steps, step_count, step->jump_target, loop_progress, budget);
        }
        loop_progress[cur] = 0;
        return explore_jump_safety(steps, step_count, cur + 1, loop_progress, budget);
    }

    int snapshot[MACRO_STEP_MAX];
    memcpy(snapshot, loop_progress, sizeof(snapshot));

    if (!explore_jump_safety(steps, step_count, step->jump_target, loop_progress, budget)) return 0;

    memcpy(loop_progress, snapshot, sizeof(snapshot));
    return explore_jump_safety(steps, step_count, cur + 1, loop_progress, budget);
}

static int
validate_jump_safety(const struct macro_step *steps, const int step_count, const int *step_line, char *error) {
    for (int start = 0; start < step_count; start++) {
        int loop_progress[MACRO_STEP_MAX] = {0};
        int budget = RELISH_SAFETY_BUDGET;

        if (!explore_jump_safety(steps, step_count, start, loop_progress, &budget)) {
            snprintf(
                error, RELISH_DEF_LENGTH,
                "Line %d: this Goto/Loop/If chain never reaches a Button step or the end of the macro (or is too "
                "complex to verify)",
                step_line[start]
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

    struct stat st;
    out_entry->created = stat(path, &st) == 0 ? (long long) st.st_mtime : 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(out_entry->compile_error, sizeof(out_entry->compile_error), "Could not open file");
        fallback_name_from_path(path, out_entry->name);
        return -1;
    }

    struct relish_label labels[RELISH_LABEL_MAX];
    int label_count = 0;
    int loop_instr_index[RELISH_LOOP_BLOCK_MAX];
    struct relish_break_ref breaks[RELISH_BREAK_MAX];
    int break_count = 0;

    if (first_pass(f, labels, &label_count, loop_instr_index, breaks, &break_count, out_entry->compile_error) != 0) {
        fclose(f);
        fallback_name_from_path(path, out_entry->name);
        return -1;
    }

    int step_line[MACRO_STEP_MAX];
    int name_set = 0;

    const int emit_result = emit_steps(
        f, labels, label_count, loop_instr_index, breaks, break_count, out_entry->steps, &out_entry->step_count,
        step_line, out_entry->name, &name_set, out_entry->compile_error
    );
    fclose(f);

    if (!name_set) fallback_name_from_path(path, out_entry->name);

    if (emit_result != 0) return -1;

    if (out_entry->step_count <= 0) {
        snprintf(
            out_entry->compile_error, sizeof(out_entry->compile_error),
            "Script has no BUTTON/PAUSE/GOTO/LOOP/IF instructions"
        );
        return -1;
    }

    if (validate_jump_safety(out_entry->steps, out_entry->step_count, step_line, out_entry->compile_error) != 0) {
        return -1;
    }

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
