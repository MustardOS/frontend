#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/display/datetime.h>
#include <common/platform/audio.h>
#include <common/platform/board.h>
#include <common/platform/device.h>
#include <common/platform/display.h>
#include <common/platform/input.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/storage/fileio.h>
#include <common/ui/common.h>
#include <common/ui/nav.h>
#include <common/ui/notify.h>
#include <module/muxshare.h>
#include "engine/document.h"
#include "menu.h"
#include "reader.h"
#include "settings.h"
#include "view.h"

#define CHUTNEY_VERSION "1.0.0"

#define REPEAT_DELAY_MS 350
#define REPEAT_RATE_MS  90
#define SCROLL_DIV      6
#define TOAST_MS        1500
#define GOTO_JUMP       10

typedef enum {
    dir_up,
    dir_down,
    dir_left,
    dir_right,
    dir_count,
} direction;

typedef struct {
    int held;
    uint32_t next;
} repeat_state;

static volatile sig_atomic_t terminate_requested = 0;

reader_settings settings;
static doc_engine *doc = NULL;
static char document_path[PATH_MAX];
static char document_title[MAX_BUFFER_SIZE];

static int page = 0;
static int page_count = 0;
static int scroll_x = 0;
static int scroll_y = 0;
static doc_image image = {0};
static int has_image = 0;
static int dirty = 0;
static int running = 1;

static doc_toc_entry *toc = NULL;
static int toc_count = 0;
static int toc_loaded = 0;

static int inhibit_published = 0;
static int reversed = 0;

static void handle_terminate(const int signal_number) {
    (void) signal_number;
    terminate_requested = 1;
}

static void init_navigation_sound(void) {
    const useconds_t backoff[] = {10000, 25000, 50000, 100000, 200000};
    for (size_t attempt = 0; attempt < A_SIZE(backoff); attempt++) {
        if (init_audio_backend()) {
            init_fe_snd(&fe_snd, config.settings.general.sound, 0);
            return;
        }
        usleep(backoff[attempt]);
    }
}

static int reflowable(void) {
    return doc_is_reflowable(doc);
}

static int plain(void) {
    return doc_is_comic(doc);
}

static void logical_size(int *width, int *height) {
    view_logical_size(width, height);
}

static int reading_height(void) {
    int width = 0;
    int height = 0;
    logical_size(&width, &height);
    const int band = settings.page_status ? view_status_height() : 0;
    return height - band > 1 ? height - band : height;
}

static int reading_width(void) {
    int width = 0;
    int height = 0;
    logical_size(&width, &height);
    return width;
}

static void forget_toc(void) {
    doc_toc_free(toc, toc_count);
    toc = NULL;
    toc_count = 0;
    toc_loaded = 0;
}

static int file_page(const int value) {
    return reversed && !reflowable() && page_count > 0 ? page_count - 1 - value : value;
}

static int clamp_page(const int value) {
    if (value < 0) return 0;
    if (value >= page_count) return page_count > 0 ? page_count - 1 : 0;
    return value;
}

static void layout(void) {
    if (reflowable()) {
        const doc_reflow reflow = {settings.text_size, settings.margin, reader_spacing_steps[settings.line_spacing]};
        page_count = doc_layout(doc, reading_width(), reading_height(), &reflow);
    } else {
        page_count = doc_page_count(doc);
    }
}

static void clamp_scroll(void) {
    if (!has_image) return;

    const int max_x = image.width - reading_width();
    const int max_y = image.height - reading_height();

    if (scroll_x > max_x) scroll_x = max_x;
    if (scroll_y > max_y) scroll_y = max_y;
    if (scroll_x < 0) scroll_x = 0;
    if (scroll_y < 0) scroll_y = 0;
}

static void compose(void) {
    char status[64] = "";
    if (settings.page_status && page_count > 0) snprintf(status, sizeof(status), "%d / %d", page + 1, page_count);

    clamp_scroll();
    view_compose(
        has_image ? &image : NULL, scroll_x, scroll_y, settings.theme, plain(), settings.page_status ? status : NULL
    );
    dirty = 1;
}

static int render(void) {
    const doc_view view = {reading_width(), reading_height(), settings.fit_page, reader_zoom_steps[settings.zoom]};
    has_image = doc_render(doc, file_page(page), &view, &image);
    if (!has_image) toast_message(lang.muxreader.page_failed, TOAST_MS);
    return has_image;
}

static void show_page(const int target, const int at_end) {
    if (page_count <= 0) return;

    const int next = clamp_page(target);
    if (next == page && has_image) return;

    view_prepare_transition(next > page ? 1 : -1);
    page = next;
    render();
    scroll_x = 0;
    scroll_y = at_end ? INT_MAX : 0;
    compose();
}

static void relayout_keep(void) {
    const int64_t mark = reflowable() ? doc_bookmark(doc, page) : 0;

    layout();
    if (reflowable()) {
        page = doc_bookmark_page(doc, mark);
        forget_toc();
    }
    page = clamp_page(page);

    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void rescale_keep_centre(void) {
    if (!has_image) {
        render();
        compose();
        return;
    }

    const double fx = ((double) scroll_x + reading_width() / 2.0) / (double) image.width;
    const double fy = ((double) scroll_y + reading_height() / 2.0) / (double) image.height;

    if (!render()) return;

    scroll_x = (int) (fx * image.width - reading_width() / 2.0);
    scroll_y = (int) (fy * image.height - reading_height() / 2.0);
    compose();
}

static void change_spacing(const int delta) {
    const int next = settings.line_spacing + delta;
    if (next < 0 || next >= reader_spacing_count) return;

    const int position = doc_position(doc, page);
    settings.line_spacing = next;

    if (!doc_set_line_spacing(doc, reader_spacing_steps[next])) {
        toast_message(lang.muxreader.open_failed, TOAST_MS);
        running = 0;
        return;
    }

    layout();
    forget_toc();
    page = doc_position_page(doc, position);
    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void change_size(const int delta) {
    if (reflowable()) {
        const int next = settings.text_size + delta * READER_TEXT_STEP;
        if (next < READER_TEXT_MIN || next > READER_TEXT_MAX) return;
        settings.text_size = next;
        relayout_keep();
    } else {
        const int next = settings.zoom + delta;
        if (next < 0 || next >= reader_zoom_count) return;
        settings.zoom = next;
        rescale_keep_centre();
    }
}

static void change_margin(const int delta) {
    const int next = settings.margin + delta * READER_MARGIN_STEP;
    if (next < 0 || next > READER_MARGIN_MAX) return;
    settings.margin = next;
    relayout_keep();
}

static void cycle_theme(const int delta) {
    settings.theme = (settings.theme + reader_theme_count + delta) % reader_theme_count;
    compose();
}

static void cycle_rotation(const int delta) {
    static const int order[reader_rotation_count] = {
        reader_rotation_none, reader_rotation_right, reader_rotation_upside, reader_rotation_left
    };

    int at = 0;
    for (int i = 0; i < reader_rotation_count; i++)
        if (order[i] == settings.rotation) at = i;

    settings.rotation = order[(at + reader_rotation_count + delta) % reader_rotation_count];
    view_set_rotation(settings.rotation);
    relayout_keep();
}

static void toggle_fit(void) {
    settings.fit_page = !settings.fit_page;
    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void toggle_status(void) {
    settings.page_status = !settings.page_status;
    relayout_keep();
}

static void scroll_page(const int delta) {
    if (!has_image) {
        show_page(page + delta, delta < 0);
        return;
    }

    const int view_h = reading_height();
    const int max_y = image.height - view_h;

    if (delta > 0 && scroll_y >= max_y) {
        show_page(page + 1, 0);
        return;
    }
    if (delta < 0 && scroll_y <= 0) {
        show_page(page - 1, 1);
        return;
    }

    int step = view_h / SCROLL_DIV;
    if (step < 1) step = 1;
    scroll_y += delta * step;
    compose();
}

static void pan_page(const int delta) {
    const int view_w = reading_width();
    if (!has_image || image.width <= view_w) {
        show_page(page + delta, 0);
        return;
    }

    int step = view_w / SCROLL_DIV;
    if (step < 1) step = 1;
    scroll_x += delta * step;
    compose();
}

static void ensure_toc(void) {
    if (toc_loaded) return;
    toc_count = doc_toc(doc, &toc);
    for (int i = 0; i < toc_count; i++)
        toc[i].page = file_page(toc[i].page);
    toc_loaded = 1;
}

static void publish_inhibit(void) {
    const int want = settings.stay_awake && !menu_active() && running;
    if (want == inhibit_published) return;

    inhibit_published = want;
    if (want) {
        write_text_to_file(IDLE_GAME_INHIBIT, "w", INT, (int) getpid());
    } else if (read_line_int_from(IDLE_GAME_INHIBIT, 1) == (int) getpid()) {
        remove(IDLE_GAME_INHIBIT);
    }
}

static int direction_pressed(const mux_input_type dpad, const mux_input_type ls, const mux_input_type rs) {
    const mux_nav_type nav = get_sticknav_mask(config.settings.advanced.stick_nav);
    return mux_input_pressed(dpad) || ((nav & NAV_LEFT_STICK) && mux_input_pressed(ls))
           || ((nav & NAV_RIGHT_STICK) && mux_input_pressed(rs));
}

static int repeat_step(repeat_state *state, const int down, const uint32_t now) {
    if (!down) {
        state->held = 0;
        return 0;
    }
    if (!state->held) {
        state->held = 1;
        state->next = now + REPEAT_DELAY_MS;
        return 1;
    }
    if (SDL_TICKS_PASSED(now, state->next)) {
        state->next = now + REPEAT_RATE_MS;
        return 1;
    }
    return 0;
}

static direction logical_direction(const direction physical) {
    if (menu_active() || settings.rotation == reader_rotation_none) return physical;

    static const direction right_map[dir_count] = {dir_left, dir_right, dir_down, dir_up};
    static const direction left_map[dir_count] = {dir_right, dir_left, dir_up, dir_down};
    static const direction upside_map[dir_count] = {dir_down, dir_up, dir_right, dir_left};

    if (settings.rotation == reader_rotation_right) return right_map[physical];
    if (settings.rotation == reader_rotation_upside) return upside_map[physical];
    return left_map[physical];
}

static void handle_direction(const direction physical) {
    const direction where = logical_direction(physical);

    if (menu_active()) {
        if (where == dir_up || where == dir_down) menu_move(where == dir_up ? -1 : 1);
        if (where == dir_left || where == dir_right) menu_change(where == dir_left ? -1 : 1);
        return;
    }

    if (where == dir_up) scroll_page(-1);
    if (where == dir_down) scroll_page(1);
    if (where == dir_left) pan_page(-1);
    if (where == dir_right) pan_page(1);
}

static void handle_shoulder(const int delta) {
    if (menu_active())
        menu_shoulder(delta);
    else
        show_page(page + delta, 0);
}

static void handle_input(void) {
    static uint64_t previous = 0;
    static repeat_state repeats[dir_count];
    static repeat_state repeat_l1;
    static repeat_state repeat_r1;
    static repeat_state repeat_a;
    static repeat_state repeat_b;

    const uint64_t mask = mux_input_pressed_mask();
    const uint64_t edge = mask & ~previous;
    previous = mask;
    const uint32_t now = SDL_GetTicks();

    if (edge & (BIT(mux_input_menu) | BIT(mux_input_start))) {
        if (menu_active()) {
            play_sound(snd_info_close);
            menu_close();
        } else {
            play_sound(snd_info_open);
            menu_open();
        }
        dirty = 1;
        return;
    }

    const int pressed[dir_count] = {
        direction_pressed(mux_input_dpad_up, mux_input_ls_up, mux_input_rs_up),
        direction_pressed(mux_input_dpad_down, mux_input_ls_down, mux_input_rs_down),
        direction_pressed(mux_input_dpad_left, mux_input_ls_left, mux_input_rs_left),
        direction_pressed(mux_input_dpad_right, mux_input_ls_right, mux_input_rs_right),
    };

    for (int i = 0; i < dir_count; i++)
        if (repeat_step(&repeats[i], pressed[i], now)) handle_direction((direction) i);

    if (repeat_step(&repeat_l1, (mask & BIT(mux_input_l1)) != 0, now)) handle_shoulder(-1);
    if (repeat_step(&repeat_r1, (mask & BIT(mux_input_r1)) != 0, now)) handle_shoulder(1);

    const int a_step = repeat_step(&repeat_a, (mask & BIT(mux_input_a)) != 0, now);
    const int b_step = repeat_step(&repeat_b, (mask & BIT(mux_input_b)) != 0, now);

    if (!menu_active()) {
        if (a_step) show_page(page + 1, 0);
        if (b_step) show_page(page - 1, 0);
        if (edge & BIT(mux_input_l2)) change_size(-1);
        if (edge & BIT(mux_input_r2)) change_size(1);
        if (edge & BIT(mux_input_x)) {
            play_sound(snd_info_open);
            menu_open_contents();
        }
        if ((edge & BIT(mux_input_y)) && !plain()) cycle_theme(1);
        if (edge & BIT(mux_input_select)) cycle_rotation(1);
        return;
    }

    if (edge & BIT(mux_input_a)) {
        play_sound(snd_confirm);
        menu_accept();
        dirty = 1;
        return;
    }

    if (edge & BIT(mux_input_b)) {
        play_sound(snd_back);
        menu_back();
        dirty = 1;
    }
}

int reader_page(void) {
    return page;
}

int reader_page_count(void) {
    return page_count;
}

int reader_reflowable(void) {
    return reflowable();
}

int reader_plain(void) {
    return plain();
}

const char *reader_title(void) {
    return document_title;
}

void reader_go_to(const int target) {
    show_page(target, 0);
}

int reader_toc(const doc_toc_entry **entries) {
    ensure_toc();
    *entries = toc;
    return toc_count;
}

void reader_change_size(const int delta) {
    change_size(delta);
}

void reader_change_margin(const int delta) {
    change_margin(delta);
}

void reader_change_spacing(const int delta) {
    change_spacing(delta);
}

void reader_cycle_theme(const int delta) {
    cycle_theme(delta);
}

void reader_cycle_rotation(const int delta) {
    cycle_rotation(delta);
}

void reader_toggle_fit(void) {
    toggle_fit();
}

void reader_toggle_status(void) {
    toggle_status();
}

void reader_set_font(const char *path) {
    snprintf(settings.font, sizeof(settings.font), "%s", path ? path : "");
    if (!reflowable()) {
        doc_set_font(doc, settings.font);
        return;
    }

    const int position = doc_position(doc, page);
    if (!doc_set_font(doc, settings.font)) {
        toast_message(lang.muxreader.open_failed, TOAST_MS);
        return;
    }

    layout();
    forget_toc();
    page = doc_position_page(doc, position);
    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

int reader_reversed(void) {
    return reversed;
}

void reader_toggle_reversed(void) {
    if (reflowable() || page_count <= 0) return;

    const int at_start = page == 0;
    const int shown = file_page(page);
    reversed = !reversed;
    forget_toc();
    page = at_start ? 0 : file_page(shown);
    reader_position_save(document_path, doc_position(doc, file_page(page)), reversed);
    if (at_start) render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

void reader_quit(void) {
    running = 0;
}

static void print_help(const char *program) {
    printf("Chutney %s\n\n", CHUTNEY_VERSION);
    printf("Usage: %s <document>\n\n", program);
    printf("Opens PDF, EPUB, CBZ, CBT and TXT documents.\n");
}

static void derive_title(void) {
    if (doc_title(doc, document_title, sizeof(document_title))) return;

    const char *base = strrchr(document_path, '/');
    snprintf(document_title, sizeof(document_title), "%s", base ? base + 1 : document_path);
    char *dot = strrchr(document_title, '.');
    if (dot && dot != document_title) *dot = '\0';
}

static void present(const int redraw) {
    const uint64_t before = display_present_serial();
    lv_task_handler();
    lv_refr_now(NULL);
    if (redraw && display_present_serial() == before) display_composite_frame();
}

static void fail_and_wait(const char *message) {
    toast_message(message, TOAST_MS * 2);
    const uint32_t until = SDL_GetTicks() + TOAST_MS * 2;
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), until) && !terminate_requested) {
        notify_tick();
        present(1);
        SDL_Delay(33);
    }
}

int main(const int argc, char **argv) {
    setlocale(LC_CTYPE, "");

    if (argc != 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0) {
        print_help(argv[0]);
        return argc == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (strcmp(argv[1], "--version") == 0) {
        printf("Chutney %s\n", CHUTNEY_VERSION);
        return EXIT_SUCCESS;
    }

    snprintf(document_path, sizeof(document_path), "%s", argv[1]);

    load_device(&device);
    load_config(&config);
    init_module("muxreader");
    init_theme(1, 0);
    init_display();
    init_navigation_sound();

    reader_settings_load(&settings);
    view_set_rotation(settings.rotation);
    view_set_transition(settings.transition);

    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer || !view_init(renderer, device.mux.width, device.mux.height)) {
        fprintf(stderr, "Chutney: cannot initialise the display\n");
        return EXIT_FAILURE;
    }

    init_ui_common_screen(&theme, &device, &lang, lang.muxreader.title);
    init_fonts();
    set_gradient_visible(0);
    lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_pnl_wall, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_grid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    menu_init();
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_terminate;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    board_init(device.board.name);
    mux_input_open();

    display_set_video_background(view_draw);
    display_set_video_background_opaque(1);
    display_set_ui_hidden(0);

    compose();
    present(1);
    fade_in_screen();

    doc = doc_open(document_path, reader_spacing_steps[settings.line_spacing]);
    if (settings.font[0] && !file_exist(settings.font)) settings.font[0] = '\0';
    if (doc && settings.font[0]) doc_set_font(doc, settings.font);
    if (doc && reflowable()) {
        toast_message(lang.muxreader.loading, tst_wait_f);
        present(1);
    }

    if (doc) layout();

    if (!doc || page_count <= 0) {
        LOG_ERROR(mux_module, "Chutney: unable to open '%s'", document_path);
        fail_and_wait(lang.muxreader.open_failed);
        mux_input_close();
        display_clear_video_background();
        doc_close(doc);
        view_free();
        sdl_cleanup_exit();
        TTF_Quit();
        return EXIT_FAILURE;
    }

    lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
    derive_title();
    const int saved = reader_position_load(document_path, &reversed);
    if (reflowable()) reversed = 0;
    page = file_page(doc_position_page(doc, saved));
    render();
    compose();

    uint32_t datetime_deadline = 0;

    while (running) {
        if (terminate_requested) running = 0;

        mux_input_poll();
        handle_input();
        publish_inhibit();

        const uint32_t now = SDL_GetTicks();
        notify_tick();
        if (SDL_TICKS_PASSED(now, datetime_deadline)) {
            datetime_task(NULL);
            datetime_deadline = now + 1000;
        }

        if (view_tick()) dirty = 1;

        const int redraw = dirty;
        dirty = 0;
        present(redraw);

        SDL_Delay(redraw || menu_active() || view_animating() ? 16 : 33);
    }

    reader_position_save(document_path, doc_position(doc, file_page(page)), reversed);
    reader_settings_save(&settings);
    running = 0;
    publish_inhibit();

    menu_close();
    forget_toc();
    mux_input_close();
    display_clear_video_background();
    doc_close(doc);
    view_free();
    sdl_cleanup_exit();
    TTF_Quit();
    return EXIT_SUCCESS;
}
