#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "../common/ui/list_frame.h"
#include "../common/task_exec.h"
#include "../common/ui/task_progress.h"
#include "ui/ui_muxspace.h"
#include "../common/ui/fs_choice.h"

#define SPACE(NAME, UDATA)          1,
#define SPACE_ROW(NAME, GLYPH, KEY) 1,
enum { ui_count_dynamic = E_SIZE(SPACE_ELEMENTS) + E_SIZE(SPACE_ROW_ELEMENTS) };
#undef SPACE_ROW
#undef SPACE

#define SPACE_INFO_ROWS  9
#define SPACE_FIRST_INFO 4

#define SPACE_BAR_WARN 70
#define SPACE_BAR_FULL 90

#define SPACE_COLOR_OK   0x4FC04F
#define SPACE_COLOR_WARN 0xE0A020
#define SPACE_COLOR_FULL 0xEE3F3F

#define SPACE_BAR_BASE_PX 24

typedef struct {
    int has_verdict;
    char verdict[32];
    char manufacturer[32];
    char model[32];
    char date[16];
} msd_info;

static int storage_present[4];
static int storage_attached[4];

static int focused_storage(void);

static void show_help(void) {
    const int index = focused_storage();
    if (index >= 0 && storage_attached[index] && !storage_present[index]) {
        show_info_box(lang.muxspace.unreadable, lang.muxspace.unreadable_hint, 0);
        return;
    }

    if (list_frame_focused()) {
        list_frame_help();
        return;
    }

    const struct help_msg help_messages[] = {
        {"primary", lang.muxspace.help.primary},   {"secondary", lang.muxspace.help.secondary},
        {"external", lang.muxspace.help.external}, {"system", lang.muxspace.help.system},
        {"total", lang.muxspace.help.total},       {"used", lang.muxspace.help.used},
        {"free", lang.muxspace.help.free},         {"filesystem", lang.muxspace.help.filesystem},
        {"node", lang.muxspace.help.node},         {"maker", lang.muxspace.help.maker},
        {"model", lang.muxspace.help.model},       {"made", lang.muxspace.help.made},
        {"quality", lang.muxspace.help.quality},
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, NULL);
}

static void init_space_bars(void) {
    lv_bar_set_range(ui_bar_primary_space, 0, 100);
    lv_bar_set_range(ui_bar_secondary_space, 0, 100);
    lv_bar_set_range(ui_bar_external_space, 0, 100);
    lv_bar_set_range(ui_bar_system_space, 0, 100);

    lv_bar_set_mode(ui_bar_primary_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_bar_secondary_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_bar_external_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_bar_system_space, LV_BAR_MODE_NORMAL);
}

static void resolve_mount(const char *mount_point, char *dev_out, char *fs_out) {
    dev_out[0] = '\0';
    fs_out[0] = '\0';

    if (!mount_point || !*mount_point) return;

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return;

    char src[128], mnt[256], fs[32];
    while (fscanf(fp, "%127s %255s %31s %*s %*d %*d\n", src, mnt, fs) == 3) {
        if (strcmp(mnt, mount_point) != 0) continue;
        snprintf(dev_out, 128, "%s", src);
        snprintf(fs_out, 32, "%s", fs);
        break;
    }

    fclose(fp);
}

static int extract_kv(const char *line, const char *key, char *out, const size_t out_len) {
    const char *p = line;
    if (strncmp(p, "INFO: ", 6) == 0 || strncmp(p, "WARN: ", 6) == 0) {
        p += 6;
    } else {
        return 0;
    }

    const size_t klen = strlen(key);
    if (strncmp(p, key, klen) != 0) return 0;

    p += klen;
    if (*p != ':') return 0;

    p++;
    while (*p == ' ')
        p++;

    snprintf(out, out_len, "%s", p);
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) {
        out[--n] = '\0';
    }

    return 1;
}

static void strip_paren_suffix(const char *s) {
    char *paren = strrchr(s, '(');
    if (!paren || paren == s) return;

    char *t = paren - 1;
    while (t > s && *t == ' ')
        t--;

    *(t + 1) = '\0';
}

static int read_msd_info(const char *dev_path, msd_info *out) {
    memset(out, 0, sizeof(*out));

    if (!*dev_path) return 0;

    const char *base = strrchr(dev_path, '/');
    base = base ? base + 1 : dev_path;
    if (strncmp(base, "mmcblk", 6) != 0) return 0;

    char block[32];
    snprintf(block, sizeof(block), "%s", base);
    char *p = strchr(block, 'p');
    if (p) *p = '\0';

    char chk_path[64];
    snprintf(chk_path, sizeof(chk_path), "/tmp/msd_check/%s", block);

    FILE *fp = fopen(chk_path, "r");
    if (!fp) return 0;

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        snprintf(out->verdict, sizeof(out->verdict), "%s", line);
        size_t n = strlen(out->verdict);

        while (n > 0 && (out->verdict[n - 1] == '\n' || out->verdict[n - 1] == ' ')) {
            out->verdict[--n] = '\0';
        }

        char *sp = strchr(out->verdict, ' ');
        if (sp) *sp = '\0';

        out->has_verdict = out->verdict[0] != '\0';
    }

    while (fgets(line, sizeof(line), fp)) {
        if (!out->manufacturer[0] && extract_kv(line, "Manufacturer", out->manufacturer, sizeof(out->manufacturer))) {
            strip_paren_suffix(out->manufacturer);
            continue;
        }

        if (!out->model[0] && extract_kv(line, "Card name", out->model, sizeof(out->model))) {
            continue;
        }

        if (!out->date[0] && extract_kv(line, "Manufacturing date", out->date, sizeof(out->date))) {
        }
    }

    fclose(fp);
    return out->has_verdict;
}

static int read_sysfs_text(const char *path, char *out, const size_t out_len) {
    out[0] = '\0';

    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    if (!fgets(out, (int) out_len, fp)) {
        fclose(fp);
        return 0;
    }

    fclose(fp);

    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) {
        out[--n] = '\0';
    }

    return out[0] != '\0';
}

static int find_usb_node(const char *start, char *out, const size_t out_len) {
    char path[PATH_MAX];
    if (!realpath(start, path)) return 0;

    for (int depth = 0; depth < 8; depth++) {
        char probe[PATH_MAX];
        snprintf(probe, sizeof(probe), "%s/idVendor", path);

        if (access(probe, R_OK) == 0) {
            snprintf(out, out_len, "%s", path);
            return 1;
        }

        char *slash = strrchr(path, '/');
        if (!slash || slash == path) break;

        *slash = '\0';
    }

    return 0;
}

static int read_usb_info(const char *dev_path, msd_info *out) {
    if (!*dev_path) return 0;

    const char *base = strrchr(dev_path, '/');
    base = base ? base + 1 : dev_path;

    if (strncmp(base, "sd", 2) != 0) return 0;

    memset(out, 0, sizeof(*out));

    char block[32];
    snprintf(block, sizeof(block), "%s", base);

    for (char *c = block; *c; c++) {
        if (*c >= '0' && *c <= '9') {
            *c = '\0';
            break;
        }
    }

    char device_dir[PATH_MAX];
    snprintf(device_dir, sizeof(device_dir), "/sys/block/%s/device", block);

    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/vendor", device_dir);
    read_sysfs_text(path, out->manufacturer, sizeof(out->manufacturer));

    snprintf(path, sizeof(path), "%s/model", device_dir);
    read_sysfs_text(path, out->model, sizeof(out->model));

    char usb_node[PATH_MAX];
    if (find_usb_node(device_dir, usb_node, sizeof(usb_node))) {
        char text[64];

        snprintf(path, sizeof(path), "%s/manufacturer", usb_node);
        if (read_sysfs_text(path, text, sizeof(text)))
            snprintf(out->manufacturer, sizeof(out->manufacturer), "%s", text);

        snprintf(path, sizeof(path), "%s/product", usb_node);
        if (read_sysfs_text(path, text, sizeof(text))) snprintf(out->model, sizeof(out->model), "%s", text);
    }

    return out->manufacturer[0] || out->model[0];
}

static const char *verdict_tag(const char *verdict) {
    if (!verdict || !*verdict) return NULL;

    if (!strcmp(verdict, "GENUINE")) return lang.muxspace.quality.genuine;
    if (!strcmp(verdict, "LIKELY_GENUINE")) return lang.muxspace.quality.likely_genuine;
    if (!strcmp(verdict, "SUSPICIOUS")) return lang.muxspace.quality.suspicious;
    if (!strcmp(verdict, "SUSPECTED_FAKE")) return lang.muxspace.quality.suspected_fake;
    if (!strcmp(verdict, "FAKE")) return lang.muxspace.quality.fake;
    if (!strcmp(verdict, "TRASH")) return lang.muxspace.quality.trash;

    return NULL;
}

typedef struct {
    lv_obj_t *value_panel;
    lv_obj_t *bar_panel;
    lv_obj_t *value;
    lv_obj_t *bar;
    lv_obj_t *title;
    const char *partition;
    const char *node;
    const char *prepare_name;
    int show_msd;
    lv_obj_t *row_value[SPACE_INFO_ROWS];
} storage_entry;

typedef struct {
    double capacity_gib;
    int partition_count;
    int potential_system_layout;
    char node[64];
} block_layout;

static storage_entry *storage_table(void) {
    static storage_entry table[4];

    table[0] = (storage_entry) {ui_pnl_primary_space,
                                ui_pnl_primary_bar_space,
                                ui_val_primary_space,
                                ui_bar_primary_space,
                                ui_lbl_primary_space,
                                device.storage.rom.mount,
                                device.storage.rom.device,
                                NULL,
                                1,
                                {ui_val_primary_total_space, ui_val_primary_used_space, ui_val_primary_free_space,
                                 ui_val_primary_fs_space, ui_val_primary_node_space, ui_val_primary_maker_space,
                                 ui_val_primary_model_space, ui_val_primary_made_space, ui_val_primary_quality_space}};

    table[1] =
        (storage_entry) {ui_pnl_secondary_space,
                         ui_pnl_secondary_bar_space,
                         ui_val_secondary_space,
                         ui_bar_secondary_space,
                         ui_lbl_secondary_space,
                         device.storage.sdcard.mount,
                         device.storage.sdcard.device,
                         "sdcard",
                         1,
                         {ui_val_secondary_total_space, ui_val_secondary_used_space, ui_val_secondary_free_space,
                          ui_val_secondary_fs_space, ui_val_secondary_node_space, ui_val_secondary_maker_space,
                          ui_val_secondary_model_space, ui_val_secondary_made_space, ui_val_secondary_quality_space}};

    table[2] =
        (storage_entry) {ui_pnl_external_space,
                         ui_pnl_external_bar_space,
                         ui_val_external_space,
                         ui_bar_external_space,
                         ui_lbl_external_space,
                         device.storage.usb.mount,
                         device.storage.usb.device,
                         "usb",
                         1,
                         {ui_val_external_total_space, ui_val_external_used_space, ui_val_external_free_space,
                          ui_val_external_fs_space, ui_val_external_node_space, ui_val_external_maker_space,
                          ui_val_external_model_space, ui_val_external_made_space, ui_val_external_quality_space}};

    table[3] = (storage_entry) {ui_pnl_system_space,
                                ui_pnl_system_bar_space,
                                ui_val_system_space,
                                ui_bar_system_space,
                                ui_lbl_system_space,
                                device.storage.root.mount,
                                device.storage.root.device,
                                NULL,
                                0,
                                {ui_val_system_total_space, ui_val_system_used_space, ui_val_system_free_space,
                                 ui_val_system_fs_space, ui_val_system_node_space, ui_val_system_maker_space,
                                 ui_val_system_model_space, ui_val_system_made_space, ui_val_system_quality_space}};

    return table;
}

static int frame_of_storage[4];
static int storage_of_frame[5];

static int node_exists(const char *node) {
    if (!node || !*node) return 0;

    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "/dev/%s", node);

    struct stat st;
    return stat(path, &st) == 0 && S_ISBLK(st.st_mode);
}

static int read_block_layout(const storage_entry *entry, block_layout *layout) {
    memset(layout, 0, sizeof(*layout));

    if (!entry || !entry->node || !*entry->node) return 0;

    const char *base = entry->node;
    if (!strncmp(base, "/dev/", 5)) base += 5;

    if (!*base || strchr(base, '/')) return 0;

    char path[PATH_MAX];
    char text[64];

    snprintf(path, sizeof(path), "/sys/class/block/%s/size", base);
    if (!read_sysfs_text(path, text, sizeof(text))) return 0;

    const unsigned long long sectors = strtoull(text, NULL, 10);
    if (!sectors) return 0;

    layout->capacity_gib = (double) sectors * 512.0 / (1024.0 * 1024.0 * 1024.0);
    snprintf(layout->node, sizeof(layout->node), "/dev/%s", base);

    FILE *fp = fopen("/proc/partitions", "r");
    if (fp) {
        char line[128];
        unsigned int major, minor;
        unsigned long long blocks;
        char name[64];
        const size_t base_len = strlen(base);

        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "%u %u %llu %63s", &major, &minor, &blocks, name) != 4) continue;
            if (strncmp(name, base, base_len) != 0) continue;

            const char *suffix = name + base_len;
            if (*suffix == 'p') suffix++;
            if (*suffix < '0' || *suffix > '9') continue;

            layout->partition_count++;
        }

        fclose(fp);
    }

    int removable = 1;
    snprintf(path, sizeof(path), "/sys/class/block/%s/removable", base);
    if (read_sysfs_text(path, text, sizeof(text))) removable = atoi(text);

    snprintf(path, sizeof(path), "/dev/%sboot0", base);
    struct stat st;
    const int has_emmc_boot = stat(path, &st) == 0 && S_ISBLK(st.st_mode);

    layout->potential_system_layout = !removable && has_emmc_boot && layout->partition_count > 1;
    return 1;
}

static void set_row_value(lv_obj_t *value, const char *text) {
    if (!value) return;

    lv_label_set_text(value, text && *text ? text : lang.generic.unknown);
}

static void update_storage_details(
    const storage_entry *entry, const int index, const double total_space, const double free_space,
    const double used_space
) {
    char text[64];

    block_layout layout;
    const int show_device_layout =
        index == 1 && read_block_layout(entry, &layout) && layout.partition_count > 1;

    if (show_device_layout) {
        snprintf(text, sizeof(text), "%.2f GB device (%.2f GB mounted)", layout.capacity_gib, total_space);
    } else {
        snprintf(text, sizeof(text), "%.2f GB", total_space);
    }
    set_row_value(entry->row_value[0], text);

    snprintf(text, sizeof(text), "%.2f GB", used_space);
    set_row_value(entry->row_value[1], text);

    snprintf(text, sizeof(text), "%.2f GB", free_space);
    set_row_value(entry->row_value[2], text);

    char dev_src[128], fs_type[32];
    resolve_mount(entry->partition, dev_src, fs_type);

    set_row_value(entry->row_value[3], fs_type);
    if (show_device_layout) {
        snprintf(text, sizeof(text), "%s (%d partitions)", dev_src, layout.partition_count);
        set_row_value(entry->row_value[4], text);
    } else {
        set_row_value(entry->row_value[4], dev_src);
    }

    msd_info msd = {0};
    if (entry->show_msd && !read_msd_info(dev_src, &msd)) read_usb_info(dev_src, &msd);

    set_row_value(entry->row_value[5], msd.manufacturer);
    set_row_value(entry->row_value[6], msd.model);
    set_row_value(entry->row_value[7], msd.date);
    set_row_value(entry->row_value[8], msd.has_verdict ? verdict_tag(msd.verdict) : NULL);

    LV_UNUSED(index);
}

static void update_storage_unreadable(const storage_entry *entry, const int index) {
    block_layout layout;
    const int show_device_layout =
        index == 1 && read_block_layout(entry, &layout) && layout.partition_count > 1;

    if (show_device_layout) {
        char text[64];
        snprintf(text, sizeof(text), "%.2f GB device", layout.capacity_gib);
        set_row_value(entry->row_value[0], text);
    } else {
        set_row_value(entry->row_value[0], NULL);
    }
    set_row_value(entry->row_value[1], NULL);
    set_row_value(entry->row_value[2], NULL);
    set_row_value(entry->row_value[3], lang.muxspace.unreadable);

    char node[MAX_BUFFER_SIZE];
    if (show_device_layout) {
        snprintf(node, sizeof(node), "%s (%d partitions)", layout.node, layout.partition_count);
    } else {
        snprintf(node, sizeof(node), "/dev/%s", entry->node ? entry->node : "");
    }
    set_row_value(entry->row_value[4], node);

    msd_info msd = {0};
    const char *device_node = show_device_layout ? layout.node : node;
    if (entry->show_msd && !read_msd_info(device_node, &msd)) read_usb_info(device_node, &msd);

    set_row_value(entry->row_value[5], msd.manufacturer);
    set_row_value(entry->row_value[6], msd.model);
    set_row_value(entry->row_value[7], msd.date);
    set_row_value(entry->row_value[8], msd.has_verdict ? verdict_tag(msd.verdict) : NULL);
}

static void update_storage_info(void) {
    const storage_entry *storage_info = storage_table();

    for (size_t i = 0; i < 4; i++) {
        double total_space, free_space, used_space;
        get_storage_info(storage_info[i].partition, &total_space, &free_space, &used_space);

        storage_present[i] = total_space > 0;
        storage_attached[i] = storage_present[i] || node_exists(storage_info[i].node);

        if (!storage_attached[i]) continue;

        if (!storage_present[i]) {
            lv_label_set_text(storage_info[i].value, lang.muxspace.unreadable);
            update_storage_unreadable(&storage_info[i], (int) i);

            lv_obj_set_height(storage_info[i].bar_panel, SPACE_BAR_BASE_PX);
            lv_bar_set_value(storage_info[i].bar, 100, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(storage_info[i].bar, lv_color_hex(SPACE_COLOR_FULL), MU_OBJ_INDI_DEFAULT);
            lv_obj_set_style_bg_opa(storage_info[i].bar, 255, MU_OBJ_INDI_DEFAULT);

            continue;
        }

        int percentage = (int) (used_space / total_space * 100.0 + 0.5);

        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;

        lv_bar_set_value(storage_info[i].bar, percentage, LV_ANIM_ON);

        char space_info[48];
        snprintf(space_info, sizeof(space_info), "%.2f GB / %.2f GB (%d%%)", used_space, total_space, percentage);
        lv_label_set_text(storage_info[i].value, space_info);

        if (lv_obj_get_height(storage_info[i].bar_panel) != SPACE_BAR_BASE_PX) {
            lv_obj_set_height(storage_info[i].bar_panel, SPACE_BAR_BASE_PX);
        }

        uint32_t bar_color;
        if (percentage >= SPACE_BAR_FULL) {
            bar_color = SPACE_COLOR_FULL;
        } else if (percentage >= SPACE_BAR_WARN) {
            bar_color = SPACE_COLOR_WARN;
        } else {
            bar_color = SPACE_COLOR_OK;
        }
        lv_obj_set_style_bg_color(storage_info[i].bar, lv_color_hex(bar_color), MU_OBJ_INDI_DEFAULT);
        lv_obj_set_style_bg_opa(storage_info[i].bar, 255, MU_OBJ_INDI_DEFAULT);

        update_storage_details(&storage_info[i], (int) i, total_space, free_space, used_space);
    }
}

static void update_storage_info_cb(const lv_timer_t *timer) {
    (void) timer;
    update_storage_info();
}

static void ui_refresh_task(lv_timer_t *timer) {
    task_progress_tick();
    ui_gen_refresh_task(timer);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, space, primary, lang.muxspace.primary, "primary", lang.muxspace.checking);
    INIT_VALUE_ITEM(-1, space, secondary, lang.muxspace.secondary, "secondary", lang.muxspace.checking);
    INIT_VALUE_ITEM(-1, space, external, lang.muxspace.external, "external", lang.muxspace.checking);
    INIT_VALUE_ITEM(-1, space, system, lang.muxspace.system, "system", lang.muxspace.checking);

#define SPACE_INFO_ITEMS(NAME)                                                                                         \
    INIT_VALUE_ITEM(-1, space, NAME##_total, lang.muxspace.detail.total, "capacity", lang.muxspace.checking);          \
    INIT_VALUE_ITEM(-1, space, NAME##_used, lang.muxspace.detail.used, "capacity", lang.muxspace.checking);            \
    INIT_VALUE_ITEM(-1, space, NAME##_free, lang.muxspace.detail.free, "capacity", lang.muxspace.checking);            \
    INIT_VALUE_ITEM(-1, space, NAME##_fs, lang.muxspace.detail.filesystem, "filesystem", lang.muxspace.checking);      \
    INIT_VALUE_ITEM(-1, space, NAME##_node, lang.muxspace.detail.device, "device", lang.muxspace.checking);            \
    INIT_VALUE_ITEM(                                                                                                   \
        -1, space, NAME##_maker, lang.muxspace.detail.manufacturer, "manufacturer", lang.muxspace.checking             \
    );                                                                                                                 \
    INIT_VALUE_ITEM(-1, space, NAME##_model, lang.muxspace.detail.model, "model", lang.muxspace.checking);             \
    INIT_VALUE_ITEM(-1, space, NAME##_made, lang.muxspace.detail.date, "date", lang.muxspace.checking);                \
    INIT_VALUE_ITEM(-1, space, NAME##_quality, lang.muxspace.detail.quality_check, "quality", lang.muxspace.checking)

#define SPACE(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_space, UDATA);
    SPACE_ELEMENTS
#undef SPACE

#define SPACE_ROW(NAME, GLYPH, KEY) lv_obj_set_user_data(ui_lbl_##NAME##_space, KEY);
    SPACE_ROW_ELEMENTS
#undef SPACE_ROW

    SPACE_INFO_ITEMS(primary);
    SPACE_INFO_ITEMS(secondary);
    SPACE_INFO_ITEMS(external);
    SPACE_INFO_ITEMS(system);
#undef SPACE_INFO_ITEMS

    for (int i = 0; i < 4; i++) {
        if (storage_attached[i]) continue;

        lv_obj_add_flag(ui_objects_panel[i], LV_OBJ_FLAG_HIDDEN);

        for (int r = 0; r < SPACE_INFO_ROWS; r++)
            lv_obj_add_flag(ui_objects_panel[SPACE_FIRST_INFO + i * SPACE_INFO_ROWS + r], LV_OBJ_FLAG_HIDDEN);
    }

    const char *section_name[] = {
        lang.muxspace.primary, lang.muxspace.secondary, lang.muxspace.external, lang.muxspace.system
    };

    list_frame frames[5];
    int frame_count = 0;

    frames[frame_count] = (list_frame) {lang.muxspace.section.all, 0, SPACE_FIRST_INFO};
    storage_of_frame[frame_count] = -1;
    frame_count++;

    for (int i = 0; i < 4; i++) {
        frame_of_storage[i] = -1;
        if (!storage_attached[i]) continue;

        frames[frame_count] = (list_frame) {section_name[i], SPACE_FIRST_INFO + i * SPACE_INFO_ROWS, SPACE_INFO_ROWS};

        frame_of_storage[i] = frame_count;
        storage_of_frame[frame_count] = i;
        frame_count++;
    }

    list_frame_init(
        &theme, ui_pnl_content, frames, frame_count, ui_objects_panel, ui_objects, ui_objects_glyph, ui_objects_value,
        ui_count_dynamic
    );

    list_frame_apply();
}

static void apply_bar_visibility(void) {
    const storage_entry *storage_info = storage_table();
    const int on_all = list_frame_current() == 0;

    for (int i = 0; i < 4; i++) {
        if (on_all && storage_attached[i]) {
            lv_obj_clear_flag(storage_info[i].bar_panel, MU_OBJ_FLAG_HIDE_FLOAT);

            const int32_t row = (int32_t) lv_obj_get_index(storage_info[i].value_panel);
            lv_obj_move_to_index(storage_info[i].bar_panel, row + 1);
        } else {
            lv_obj_add_flag(storage_info[i].bar_panel, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, 0, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void section_changed(void) {
    apply_bar_visibility();
    update_storage_info();

    nav_moved = 1;
}

static int focused_storage(void) {
    const int frame = list_frame_current();
    if (frame < 1 || frame >= (int) A_SIZE(storage_of_frame)) return -1;

    return storage_of_frame[frame];
}

static const char *prepare_target(void) {
    const int index = focused_storage();
    if (index < 0 || !storage_attached[index]) return NULL;

    return storage_table()[index].prepare_name;
}

static const char *prepare_label(void) {
    switch (focused_storage()) {
        case 1:
            return lang.muxspace.secondary;
        case 2:
            return lang.muxspace.external;
        default:
            return "";
    }
}

static int prepare_system_layout(block_layout *layout) {
    const int index = focused_storage();
    if (index != 1) return 0;

    return read_block_layout(&storage_table()[index], layout) && layout->potential_system_layout;
}

static mux_dialogue format_dlg;
static mux_dialogue system_layout_dlg;
static mux_dialogue warn_dlg;
static mux_dialogue final_dlg;
static mux_dialogue commit_dlg;

static int task_pending = 0;
static const char *chosen_fs = NULL;

#define MATH_CHOICES 6

static mux_dialogue math_dlg;
static char math_labels[MATH_CHOICES][16];
static int math_answer = 0;
static int math_invert = 0;

static void run_prepare_task(void) {
    const char *target = prepare_target();
    if (!target || !chosen_fs) return;

    const char *argv[] = {OPT_PATH "script/system/prepare.sh", target, chosen_fs, NULL};

    const task_exec_spec spec = {
        .argv = argv,
        .argc = 3,
        .mode = task_mode_progress,
        .can_cancel = 0,
        .turbo = 1,
        .title = lang.muxspace.prepare.task,
    };

    if (task_exec_start(&spec) == 0) {
        task_pending = 1;
        task_progress_show();
    }
}

static void ask_prepare_warning(void) {
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), lang.muxspace.prepare.warning, prepare_label());

    dialogue_init_confirm(
        &warn_dlg, &theme, ui_screen, lang.muxspace.prepare.title, message, lang.generic.yes, lang.generic.no,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_open(&warn_dlg, &theme);
}

static void ask_prepare_system_layout(void) {
    block_layout layout;
    if (!prepare_system_layout(&layout)) {
        ask_prepare_warning();
        return;
    }

    char message[MAX_BUFFER_SIZE];
    snprintf(
        message, sizeof(message), lang.muxspace_system_layout, prepare_label(), layout.partition_count,
        layout.capacity_gib
    );

    dialogue_init_confirm(
        &system_layout_dlg, &theme, ui_screen, lang.muxspace.prepare.title, message, lang.generic.yes, lang.generic.no,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_open(&system_layout_dlg, &theme);
}

static void ask_prepare_final(void) {
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), lang.muxspace.prepare.final, prepare_label());

    dialogue_init_confirm(
        &final_dlg, &theme, ui_screen, lang.muxspace.prepare.title, message, lang.generic.yes, lang.generic.no,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_open(&final_dlg, &theme);
}

static int math_holds(const int *taken, const int count, const int value) {
    for (int i = 0; i < count; i++)
        if (taken[i] == value) return 1;

    return 0;
}

static void ask_prepare_math(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL));
        seeded = 1;
    }

    const int a = 2 + (int) (random() % 11);
    const int b = 2 + (int) (random() % 11);
    const int c = 2 + (int) (random() % 9);
    const int m = 2 + (int) (random() % 4);

    const int hi = a > b ? a : b;
    const int lo = a > b ? b : a;

    char question[64];
    int result;

    switch (random() % 5) {
        case 0:
            result = (a + b) * m - c;
            snprintf(question, sizeof(question), "(%d + %d) x %d - %d = ?", a, b, m, c);
            break;
        case 1:
            result = (hi - lo) * m + c;
            snprintf(question, sizeof(question), "(%d - %d) x %d + %d = ?", hi, lo, m, c);
            break;
        case 2:
            result = a * m - (b + c);
            snprintf(question, sizeof(question), "%d x %d - (%d + %d) = ?", a, m, b, c);
            break;
        case 3:
            result = (a + b) * (m - 1) - lo;
            snprintf(question, sizeof(question), "(%d + %d) x %d - %d = ?", a, b, m - 1, lo);
            break;
        default:
            result = a + b * m - c;
            snprintf(question, sizeof(question), "%d + %d x %d - %d = ?", a, b, m, c);
            break;
    }

    int values[MATH_CHOICES];
    int taken[MATH_CHOICES] = {result};
    int count = 1;

    math_answer = (int) (random() % MATH_CHOICES);
    values[math_answer] = result;

    for (int i = 0; i < MATH_CHOICES; i++) {
        if (i == math_answer) continue;

        int guess;
        do {
            const int drift = 1 + (int) (random() % 12);
            guess = random() % 2 ? result + drift : result - drift;
        } while (math_holds(taken, count, guess));

        taken[count++] = guess;
        values[i] = guess;
    }

    const char *labels[MATH_CHOICES];
    for (int i = 0; i < MATH_CHOICES; i++) {
        snprintf(math_labels[i], sizeof(math_labels[i]), "%d", values[i]);
        labels[i] = math_labels[i];
    }

    math_invert = random() % 20 == 0;

    char message[MAX_BUFFER_SIZE];
    snprintf(
        message, sizeof(message),
        math_invert ? lang.muxspace.prepare.challenge_invert : lang.muxspace.prepare.challenge, question
    );

    dialogue_init(
        &math_dlg, &theme, ui_screen, lang.muxspace.prepare.title, message, labels, MATH_CHOICES, lang.generic.select,
        lang.generic.cancel
    );

    dialogue_open(&math_dlg, &theme);
}

static fs_choice_text format_text(void) {
    return (fs_choice_text) {
        .title = lang.muxspace.prepare.title,
        .description = lang.muxspace.prepare.choose,
        .no_tooling = lang.muxspace.prepare.no_tooling,
        .name = {lang.muxspace.prepare.vfat, lang.muxspace.prepare.exfat, lang.muxspace.prepare.ext4},
        .about = {
            lang.muxspace.prepare.about_vfat, lang.muxspace.prepare.about_exfat, lang.muxspace.prepare.about_ext4
        },
    };
}

static void format_describe(void) {
    const fs_choice_text text = format_text();
    fs_choice_describe(&format_dlg, &text);
}

static void ask_prepare_commit(void) {
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), lang.muxspace.prepare.commit, prepare_label(), chosen_fs ? chosen_fs : "");

    dialogue_init_confirm(
        &commit_dlg, &theme, ui_screen, lang.muxspace.prepare.title, message, lang.generic.yes, lang.generic.no,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_open(&commit_dlg, &theme);
}

static void ask_prepare_format(void) {
    const fs_choice_text text = format_text();
    fs_choice_open(&format_dlg, &theme, ui_screen, &text);
}

static void nav_refresh(void);
static void format_describe(void);

static mux_dialogue *active_dialogue(void) {
    if (dialogue_active(&math_dlg)) return &math_dlg;
    if (dialogue_active(&format_dlg)) return &format_dlg;
    if (dialogue_active(&system_layout_dlg)) return &system_layout_dlg;
    if (dialogue_active(&warn_dlg)) return &warn_dlg;
    if (dialogue_active(&final_dlg)) return &final_dlg;
    if (dialogue_active(&commit_dlg)) return &commit_dlg;

    return NULL;
}

static int dialogue_step(const int direction) {
    mux_dialogue *dlg = active_dialogue();
    if (!dlg) return 0;

    dialogue_handle_dpad(dlg, &theme, direction, 1);
    if (dlg == &format_dlg) format_describe();

    return 1;
}

static void handle_dpad_up(void) {
    if (task_progress_handle_dpad(-1)) return;
    if (dialogue_step(-1)) return;

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (task_progress_handle_dpad(+1)) return;
    if (dialogue_step(+1)) return;

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (task_progress_handle_dpad_hold(-1)) return;
    if (dialogue_step(-1)) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (task_progress_handle_dpad_hold(+1)) return;
    if (dialogue_step(+1)) return;

    handle_list_nav_down_hold();
}

static void finish_prepare(void) {
    task_pending = 0;
    task_exec_acknowledge();

    chosen_fs = NULL;

    update_storage_info();
    apply_bar_visibility();

    nav_moved = 1;
}

static void handle_a(void) {
    if (task_progress_handle_a()) {
        if (task_pending && !task_progress_active()) finish_prepare();
        return;
    }

    if (dialogue_active(&math_dlg)) {
        const int picked_answer = math_dlg.selected == math_answer;
        const int correct = math_invert ? !picked_answer : picked_answer;

        dialogue_mark_silent(&math_dlg);
        dialogue_dismiss(&math_dlg);

        if (!correct) {
            play_sound(snd_error);
            toast_message(lang.muxspace.prepare.wrong, tst_wait_m);

            return;
        }

        play_sound(snd_muos);
        ask_prepare_format();

        return;
    }

    if (dialogue_active(&format_dlg)) {
        const int slot = format_dlg.selected;
        dialogue_dismiss(&format_dlg);

        chosen_fs = fs_choice_name(slot);
        if (!chosen_fs) return;

        block_layout layout;
        if (prepare_system_layout(&layout)) {
            ask_prepare_system_layout();
        } else {
            ask_prepare_warning();
        }
        return;
    }

    if (dialogue_active(&system_layout_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) system_layout_dlg.selected;
        dialogue_dismiss(&system_layout_dlg);

        if (opt == mux_confirm_yep) ask_prepare_warning();
        return;
    }

    if (dialogue_active(&warn_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) warn_dlg.selected;
        dialogue_dismiss(&warn_dlg);

        if (opt == mux_confirm_yep) ask_prepare_final();
        return;
    }

    if (dialogue_active(&final_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) final_dlg.selected;
        dialogue_dismiss(&final_dlg);

        if (opt == mux_confirm_yep) ask_prepare_commit();
        return;
    }

    if (dialogue_active(&commit_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) commit_dlg.selected;
        dialogue_dismiss(&commit_dlg);

        if (opt == mux_confirm_yep) run_prepare_task();
        return;
    }

    if (msgbox_active || progress_onscreen != -1 || hold_call) return;

    if (list_frame_focused()) {
        play_sound(snd_info_open);
        list_frame_help();

        return;
    }

    if (list_frame_current() != 0) return;

    const int row = list_frame_current_row();
    if (row < 0 || row >= SPACE_FIRST_INFO) return;

    const int frame = frame_of_storage[row];
    if (frame < 0 || !list_frame_go(frame)) return;

    play_sound(snd_confirm);
    section_changed();
    nav_refresh();
}

static void handle_b(void) {
    if (task_progress_handle_b()) return;

    if (dialogue_active(&math_dlg)) {
        dialogue_cancel(&math_dlg);
        return;
    }

    if (dialogue_active(&format_dlg)) {
        dialogue_cancel(&format_dlg);
        return;
    }

    if (dialogue_active(&system_layout_dlg)) {
        dialogue_cancel(&system_layout_dlg);
        return;
    }

    if (dialogue_active(&warn_dlg)) {
        dialogue_cancel(&warn_dlg);
        return;
    }

    if (dialogue_active(&final_dlg)) {
        dialogue_cancel(&final_dlg);
        return;
    }

    if (dialogue_active(&commit_dlg)) {
        dialogue_cancel(&commit_dlg);
        return;
    }

    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "space");

    mux_input_stop();
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (msgbox_active || progress_onscreen != -1 || hold_call) return;
    if (task_progress_active() || active_dialogue()) return;
    if (!prepare_target()) return;

    play_sound(snd_confirm);
    ask_prepare_math();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;
    if (task_progress_active()) return;

    play_sound(snd_info_open);
    show_help();
}

static void nav_refresh(void) {
    if (list_frame_current() == 0) {
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (prepare_target()) {
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void handle_section_prev(void) {
    if (task_progress_active()) return;
    if (dialogue_step(-1)) return;

    if (list_frame_move(-1)) {
        play_sound(snd_navigate);
        section_changed();
        nav_refresh();
    }
}

static void handle_section_next(void) {
    if (task_progress_active()) return;
    if (dialogue_step(+1)) return;

    if (list_frame_move(+1)) {
        play_sound(snd_navigate);
        section_changed();
        nav_refresh();
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.muxspace.prepare.nav, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxspace_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxspace.title);

    init_muxspace(ui_pnl_content);
    init_elements();
    init_space_bars();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();

    update_storage_info();
    init_navigation_group();
    update_storage_info();

    apply_bar_visibility();
    nav_refresh();

    task_progress_init(&theme, ui_screen);
    init_timer(ui_refresh_task, update_storage_info_cb);
    list_nav_next(0);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_dpad_left] = handle_section_prev,
                [mux_input_dpad_right] = handle_section_next,
                [mux_input_l1] = handle_section_prev,
                [mux_input_r1] = handle_section_next,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_section_prev,
            [mux_input_r1] = handle_section_next,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxspace.title, lang.muxspace.overview);

    mux_input_task(&input_opts);

    return 0;
}
