#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/runtime/init.h>
#include <common/config/config.h>
#include <common/display/theme.h>
#include <common/ui/nav/menu_style.h>

#define ORBIT_SLOTS  9
#define BUBBLE_COUNT 8

static char style_json[8192];
static int style_json_ready = 0;

static const char *bubble_names[BUBBLE_COUNT] = {"explore", "collection", "history", "apps",
                                                 "info",    "config",     "reboot",  "shutdown"};

int menu_style_active(void) {
    return config.settings.themeopt.menu_style > menu_style_theme && strcmp(mux_module, "muxlaunch") == 0;
}

const char *menu_style_layout_json(void) {
    return style_json_ready ? style_json : NULL;
}

static size_t json_slot(
    char *out, const size_t size, const int x, const int y, const int w, const int h, const int radius,
    const int rotate_y, const int first
) {
    const int written = snprintf(
        out, size, "%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"radius\":%d,\"rotate_y\":%d}", first ? "" : ",", x, y, w,
        h, radius, rotate_y
    );
    return written > 0 && (size_t) written < size ? (size_t) written : 0;
}

static void set_cells(
    struct theme_config *theme, const int rows, const int columns, const int column_width, const int row_height,
    const int cell, const int x, const int y
) {
    theme->grid.row_count = (int16_t) rows;
    theme->grid.column_count = (int16_t) columns;
    theme->grid.column_width = (int16_t) column_width;
    theme->grid.row_height = (int16_t) row_height;
    theme->grid.location_x = (int16_t) x;
    theme->grid.location_y = (int16_t) y;
    theme->grid.cell.width = (int16_t) cell;
    theme->grid.cell.height = (int16_t) cell;
    theme->grid.cell.column_align = 1;
    theme->grid.cell.row_align = 1;
    theme->grid.cell.radius = (int16_t) (cell * 3 / 10);
    theme->grid.carousel_fade = 0;
    theme->grid.layout_slot_gap = 0;
}

static void apply_grid(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int label = theme->grid.current_item_label.height + abs(theme->grid.current_item_label.offset_y);
    const int content = height - top - (label > theme->footer.height ? label : theme->footer.height);
    const int column_width = width * 95 / 400;
    const int row_height = content / 2;
    const int cell = (int) fmin(column_width * 0.88, row_height * 0.88);
    set_cells(
        theme, 2, 4, column_width, row_height, cell, (width - column_width * 4) / 2,
        top + (content - row_height * 2) / 2
    );
}

static int carousel_columns(const int width, const int height) {
    return width * 2 >= height * 3 ? 5 : 3;
}

static void apply_carousel(
    struct theme_config *theme, const int width, const int height, int *columns, int *column_width, int *cell
) {
    *columns = carousel_columns(width, height);
    *column_width = (int) (width * 0.94 / *columns);
    *cell = (int) fmin(*column_width * 0.84, height * 0.40);
    set_cells(
        theme, 1, *columns, *column_width, (int) (*cell * 1.15), *cell, (width - *columns * *column_width) / 2,
        (int) ((height - *cell) / 2.0 - height * 0.05)
    );
    theme->grid.carousel_fade = 30;
    theme->grid.layout_slot_gap = (int16_t) lround(36.0 * height / 1080.0);
}

static void build_coverflow(const int columns, const int column_width, const int cell) {
    static const double sizes[] = {1.25, 0.95, 0.80};
    const double turn = 50.0;
    const double gap = cell * 0.05;
    const double squash = cos(turn * M_PI / 180.0);
    const int middle = columns / 2;
    const int centre_y = (int) (cell * 1.25 / 2);

    size_t used = (size_t) snprintf(style_json, sizeof(style_json), "{\"mode\":\"slots\",\"reflect\":70,\"slots\":[");
    for (int index = 0; index < columns; index++) {
        const int distance = abs(index - middle);
        const int size = (int) (cell * sizes[distance]);
        double offset = 0.0;
        for (int step = 1; step <= distance; step++) {
            const double previous = cell * sizes[step - 1] * (step == 1 ? 1.0 : squash);
            offset += previous / 2 + cell * sizes[step] * squash / 2 + gap;
        }
        const int side = index > middle ? 1 : -1;
        const double centre_x = columns * column_width / 2.0 + side * offset;
        used += json_slot(
            style_json + used, sizeof(style_json) - used, (int) (centre_x - size / 2.0), (int) (centre_y - size / 2.0),
            size, size, (int) (size * 0.22), distance ? (int) (-side * turn) : 0, index == 0
        );
    }
    snprintf(style_json + used, sizeof(style_json) - used, "]}");
}

static void apply_bubbles(struct theme_config *theme, const int width, const int height) {
    const int diameter = (int) fmin(height * 0.21, width * 0.135);
    const double step = fmin(width * 0.205, diameter * 1.9);
    const double left = (width - step * 3.5) / 2;
    const double rows[] = {height * 0.30, height * 0.66};

    set_cells(theme, 2, 4, (int) step, (int) (height * 0.36), diameter, 0, 0);
    theme->grid.cell.radius = (int16_t) (diameter / 2);

    size_t used = (size_t) snprintf(style_json, sizeof(style_json), "{\"mode\":\"items\",\"items\":{");
    for (int index = 0; index < BUBBLE_COUNT; index++) {
        const int row = index / 4;
        const double centre_x = left + step * (index % 4 + row * 0.5);
        const int written = snprintf(
            style_json + used, sizeof(style_json) - used,
            "%s\"%s\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"radius\":%d}", index ? "," : "", bubble_names[index],
            (int) (centre_x - diameter / 2.0), (int) (rows[row] - diameter / 2.0), diameter, diameter, diameter / 2
        );
        if (written > 0 && (size_t) written < sizeof(style_json) - used) used += (size_t) written;
    }
    snprintf(style_json + used, sizeof(style_json) - used, "}}");
}

static void apply_orbit(struct theme_config *theme, const int width, const int height) {
    const double front = fmin(height * 0.28, width * 0.16);
    const double centre_x = width / 2.0;
    const double centre_y = height * 0.43;
    const double radius_x = width * 0.40;
    const double radius_y = height * 0.22;
    const int middle = ORBIT_SLOTS / 2;

    set_cells(theme, 1, ORBIT_SLOTS, width / ORBIT_SLOTS, (int) (height * 0.8), (int) front, 0, 0);
    theme->grid.carousel_fade = 20;
    theme->grid.animation_time = 420;

    size_t used =
        (size_t) snprintf(style_json, sizeof(style_json), "{\"mode\":\"slots\",\"motion\":\"orbit\",\"slots\":[");
    for (int index = 0; index < ORBIT_SLOTS; index++) {
        const int offset = index - middle;
        const double angle = offset * 2 * M_PI / (ORBIT_SLOTS - 1);
        const double nearness = (1 + cos(angle)) / 2;
        const int size = (int) (front * (0.40 + 0.60 * nearness));
        used += json_slot(
            style_json + used, sizeof(style_json) - used, (int) lround(centre_x + radius_x * sin(angle) - size / 2.0),
            (int) lround(centre_y + radius_y * cos(angle) - size / 2.0), size, size, (int) (size * 0.30),
            offset ? (int) lround(-35 * sin(angle)) : 0, index == 0
        );
    }
    snprintf(style_json + used, sizeof(style_json) - used, "]}");
}

void menu_style_apply(struct theme_config *theme, const int width, const int height) {
    style_json_ready = 0;
    if (!menu_style_active()) return;

    int columns, column_width, cell;
    switch (config.settings.themeopt.menu_style) {
        case menu_style_list:
            theme->grid.row_count = 0;
            theme->grid.column_count = 0;
            break;
        case menu_style_grid:
            apply_grid(theme, width, height);
            break;
        case menu_style_carousel:
            apply_carousel(theme, width, height, &columns, &column_width, &cell);
            break;
        case menu_style_coverflow:
            apply_carousel(theme, width, height, &columns, &column_width, &cell);
            build_coverflow(columns, column_width, cell);
            style_json_ready = 1;
            break;
        case menu_style_bubbles:
            apply_bubbles(theme, width, height);
            style_json_ready = 1;
            break;
        case menu_style_orbit:
            apply_orbit(theme, width, height);
            style_json_ready = 1;
            break;
        default:
            break;
    }
}
