#include <string.h>
#include <common/runtime/init.h>
#include <common/config/config.h>
#include <common/display/theme.h>
#include <common/ui/nav/grid_nav.h>
#include <common/ui/nav/menu_style.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/grid.h>
#include <common/ui/nav/style/carousel.h>
#include <common/ui/nav/style/coverflow.h>
#include <common/ui/nav/style/bubbles.h>
#include <common/ui/nav/style/orbit.h>
#include <common/ui/nav/style/dial.h>
#include <common/ui/nav/style/stack.h>
#include <common/ui/nav/style/clock.h>
#include <common/ui/nav/style/rolodex.h>
#include <common/ui/nav/style/dock.h>
#include <common/ui/nav/style/fan.h>
#include <common/ui/nav/style/honeycomb.h>
#include <common/ui/nav/style/tiles.h>
#include <common/ui/nav/style/sidebar.h>
#include <common/ui/nav/style/fluent.h>
#include <common/ui/nav/style/spotlight.h>
#include <common/ui/nav/style/banners.h>
#include <common/ui/nav/style/crossbar.h>
#include <common/ui/nav/style/desktop.h>
#include <common/ui/nav/style/tabbar.h>
#include <common/ui/nav/style/blades.h>

typedef void (*menu_style_fn)(struct theme_config *theme, int width, int height);

static const menu_style_fn style_apply[] = {
    [menu_style_grid] = style_grid_apply,
    [menu_style_carousel] = style_carousel_apply,
    [menu_style_coverflow] = style_coverflow_apply,
    [menu_style_bubbles] = style_bubbles_apply,
    [menu_style_orbit] = style_orbit_apply,
    [menu_style_dial] = style_dial_apply,
    [menu_style_stack] = style_stack_apply,
    [menu_style_clock] = style_clock_apply,
    [menu_style_rolodex] = style_rolodex_apply,
    [menu_style_dock] = style_dock_apply,
    [menu_style_fan] = style_fan_apply,
    [menu_style_honeycomb] = style_honeycomb_apply,
    [menu_style_tiles] = style_tiles_apply,
    [menu_style_sidebar] = style_sidebar_apply,
    [menu_style_fluent] = style_fluent_apply,     [menu_style_spotlight] = style_spotlight_apply,
    [menu_style_banners] = style_banners_apply,   [menu_style_crossbar] = style_crossbar_apply,
    [menu_style_desktop] = style_desktop_apply,   [menu_style_tabbar] = style_tabbar_apply,
    [menu_style_blades] = style_blades_apply,
};

static int theme_allows_styles = 1;

int menu_style_active(void) {
    return theme_allows_styles && config.settings.themeopt.menu_style > menu_style_theme
           && strcmp(mux_module, "muxlaunch") == 0;
}

const int *menu_item_order(void) {
    static int order[MENU_ITEM_COUNT];
    int seen[MENU_ITEM_COUNT] = {0};
    int used = 0;

    for (const char *c = theme_allows_styles ? config.settings.themeopt.menu_order : ""; *c && used < MENU_ITEM_COUNT;
         c++) {
        const int item = *c - '0';
        if (item < 0 || item >= MENU_ITEM_COUNT || seen[item]) continue;
        seen[item] = 1;
        order[used++] = item;
    }

    for (int item = 0; item < MENU_ITEM_COUNT; item++) {
        if (!seen[item]) order[used++] = item;
    }

    return order;
}

int menu_style_any_direction(void) {
    if (!menu_style_active()) return 0;

    switch (config.settings.themeopt.menu_style) {
        case menu_style_clock:
        case menu_style_rolodex:
        case menu_style_sidebar:
        case menu_style_banners:
            return 1;
        default:
            return 0;
    }
}

int menu_style_spatial(void) {
    if (!menu_style_active()) return 0;

    switch (config.settings.themeopt.menu_style) {
        case menu_style_grid:
        case menu_style_bubbles:
        case menu_style_honeycomb:
        case menu_style_tiles:
        case menu_style_fluent:
        case menu_style_desktop:
            return 1;
        default:
            return 0;
    }
}

const char *menu_style_layout_json(void) {
    return menu_style_active() ? style_json() : NULL;
}

void menu_style_apply(struct theme_config *theme, const int width, const int height) {
    style_json_reset();
    if (strcmp(mux_module, "muxlaunch") == 0) {
        const int list_hidden = theme->list_default.text_alpha == 0 && theme->list_focus.text_alpha == 0;
        theme_allows_styles = theme->misc.menu_style >= 0 ? theme->misc.menu_style > 0 : !list_hidden;
    }
    if (!menu_style_active()) return;

    const int style = config.settings.themeopt.menu_style;
    if (style == menu_style_list) {
        theme->grid.row_count = 0;
        theme->grid.column_count = 0;
        theme->misc.navigation_type = nav_standard;
        return;
    }

    if (style < (int) (sizeof(style_apply) / sizeof(style_apply[0])) && style_apply[style]) {
        style_apply[style](theme, width, height);
    }
}
