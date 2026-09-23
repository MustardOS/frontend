#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/display/datetime.h>
#include <common/platform/board.h>
#include <common/platform/device.h>
#include <common/platform/display.h>
#include <common/platform/input.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/image.h>
#include <common/ui/notify.h>
#include <module/muxshare.h>
#include "config.h"
#include "input.h"
#include "menu.h"
#include "osk.h"
#include "render.h"
#include "vt.h"

static volatile sig_atomic_t child_exited = 0;
static volatile sig_atomic_t terminate_requested = 0;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *terminal_texture = NULL;
static SDL_Texture *background_texture = NULL;
static TTF_Font *terminal_fonts[8] = {NULL};
static MuxtermConfig term_config;
static int cell_width = 8;
static int cell_height = 16;
static int terminal_columns = 1;
static int terminal_rows = 1;
static int visible_rows = 1;
static int pty_fd = -1;
static pid_t child_pid = -1;

static void sigchld_handler(const int signal_number) {
    (void) signal_number;
    child_exited = 1;
}

static void terminate_handler(const int signal_number) {
    (void) signal_number;
    terminate_requested = 1;
}

static void on_title_change(const char *title, void *userdata) {
    (void) userdata;
    if (title && *title) SDL_SetWindowTitle(display_get_window(), title);
}

static void draw_terminal(SDL_Renderer *target) {
    if (!terminal_texture) return;
    display_render_logical_texture(target, terminal_texture);
}

static int parse_hex_colour(const char *hex, SDL_Color *out) {
    if (!hex) return 0;
    while (*hex && isspace((unsigned char) *hex))
        hex++;
    if (*hex == '#') hex++;

    unsigned int red, green, blue;
    if (strlen(hex) < 6 || sscanf(hex, "%2x%2x%2x", &red, &green, &blue) != 3) return 0;
    *out = (SDL_Color) {(Uint8) red, (Uint8) green, (Uint8) blue, 255};
    return 1;
}

static int parse_font_hinting(const char *value) {
    if (strcmp(value, "light") == 0) return 1;
    if (strcmp(value, "mono") == 0) return 2;
    if (strcmp(value, "none") == 0) return 3;
    return 0;
}

static void apply_theme_defaults(void) {
    if (!term_config.term_font_size_explicit) {
        const int size = atoi(theme.terminal.font_size);
        if (size > 0) term_config.term_font_size = size;
    }
    if (!term_config.font_hinting_explicit) term_config.font_hinting = parse_font_hinting(theme.terminal.font_hint);
    if (!term_config.foreground_explicit && parse_hex_colour(theme.terminal.foreground, &term_config.solid_fg))
        term_config.use_solid_fg = 1;
    if (!term_config.background_explicit && parse_hex_colour(theme.terminal.background, &term_config.solid_bg))
        term_config.use_solid_bg = 1;

    if (!term_config.term_font_path_explicit) {
        char path[PATH_MAX];
        if (load_terminal_resource("font", "ttf", path, sizeof(path)))
            snprintf(term_config.term_font_path, sizeof(term_config.term_font_path), "%s", path);
    }
    if (!term_config.background_image_explicit) {
        char path[PATH_MAX];
        if (load_terminal_resource("image", "png", path, sizeof(path)))
            snprintf(term_config.bg_image, sizeof(term_config.bg_image), "%s", path);
    }
}

static TTF_Font *open_font_slot(const int slot) {
    const int bold = (slot & 1) != 0;
    const int underline = (slot & 2) != 0;
    const int italic = (slot & 4) != 0;
    const char *explicit_path = NULL;

    if (bold && italic && term_config.term_font_path_bold_italic[0]) {
        explicit_path = term_config.term_font_path_bold_italic;
    } else if (italic && !bold && term_config.term_font_path_italic[0]) {
        explicit_path = term_config.term_font_path_italic;
    } else if (bold && !italic && term_config.term_font_path_bold[0]) {
        explicit_path = term_config.term_font_path_bold;
    }

    TTF_Font *font =
        TTF_OpenFont(explicit_path ? explicit_path : term_config.term_font_path, term_config.term_font_size);
    if (!font) return NULL;

    TTF_SetFontHinting(font, term_config.font_hinting);
    if (!explicit_path) {
        int style = TTF_STYLE_NORMAL;
        if (bold) style |= TTF_STYLE_BOLD;
        if (underline) style |= TTF_STYLE_UNDERLINE;
        if (italic) style |= TTF_STYLE_ITALIC;
        TTF_SetFontStyle(font, style);
    }
    return font;
}

static int reload_terminal_fonts(void) {
    TTF_Font *replacement[8] = {NULL};
    for (int i = 0; i < 8; i++)
        replacement[i] = open_font_slot(i);

    if (!replacement[0]) {
        for (int i = 1; i < 8; i++)
            if (replacement[i]) TTF_CloseFont(replacement[i]);
        return 0;
    }

    int width = 0;
    int height = 0;
    TTF_SizeUTF8(replacement[0], "M", &width, &height);
    if (width < 1) width = 8;
    if (height < 1) height = 16;

    for (int i = 0; i < 8; i++) {
        if (terminal_fonts[i]) TTF_CloseFont(terminal_fonts[i]);
        terminal_fonts[i] = replacement[i];
    }

    cell_width = width;
    cell_height = height;
    render_init(terminal_fonts, cell_width, cell_height, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
    render_glyph_cache_clear();
    return 1;
}

static void resize_pty(void) {
    vt_resize(terminal_columns, terminal_rows);
    const struct winsize size = {
        .ws_row = (unsigned short) terminal_rows,
        .ws_col = (unsigned short) terminal_columns,
    };
    if (pty_fd >= 0) ioctl(pty_fd, TIOCSWINSZ, &size);
    if (child_pid > 0) kill(child_pid, SIGWINCH);
}

static int recreate_terminal_texture(void) {
    terminal_columns = term_config.width / cell_width;
    terminal_rows = term_config.height / cell_height;
    if (terminal_columns < 1) terminal_columns = 1;
    if (terminal_rows < 1) terminal_rows = 1;
    visible_rows = terminal_rows;

    SDL_Texture *replacement = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, terminal_columns * cell_width,
        terminal_rows * cell_height
    );
    if (!replacement) return 0;

    if (terminal_texture) SDL_DestroyTexture(terminal_texture);
    terminal_texture = replacement;
    resize_pty();
    osk_update_metrics(term_config.width, cell_height);
    vt_scroll_set(0);
    vt_mark_all_rows_dirty();
    return 1;
}

static pid_t spawn_child(int *master_out, const int argc, char **argv) {
    int master = -1;
    int slave = -1;
    const struct winsize size = {
        .ws_row = (unsigned short) terminal_rows,
        .ws_col = (unsigned short) terminal_columns,
    };

    if (openpty(&master, &slave, NULL, NULL, &size) < 0) return -1;

    const pid_t pid = fork();
    if (pid < 0) {
        close(master);
        close(slave);
        return -1;
    }

    if (pid == 0) {
        setsid();
        ioctl(slave, TIOCSCTTY, 0);
        if (dup2(slave, STDIN_FILENO) < 0 || dup2(slave, STDOUT_FILENO) < 0 || dup2(slave, STDERR_FILENO) < 0)
            _exit(EXIT_FAILURE);

        close(master);
        close(slave);
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("IS_MUTERM", "1", 1);
        if (!getenv("HOME")) setenv("HOME", "/root", 1);

        char columns[16];
        char rows[16];
        snprintf(columns, sizeof(columns), "%d", terminal_columns);
        snprintf(rows, sizeof(rows), "%d", terminal_rows);
        setenv("COLUMNS", columns, 1);
        setenv("LINES", rows, 1);

        if (argc > 0 && argv && argv[0] && argv[0][0]) {
            execvp(argv[0], argv);
            _exit(127);
        }

        const char *shell = term_config.shell[0] ? term_config.shell : getenv("SHELL");
        if (!shell || !*shell) shell = "/bin/sh";
        execlp(shell, shell, "-l", (char *) NULL);
        _exit(127);
    }

    close(slave);
    *master_out = master;
    return pid;
}

static int reap_child(const pid_t pid, const int attempts) {
    for (int i = 0; i < attempts; i++) {
        const pid_t result = waitpid(pid, NULL, WNOHANG);
        if (result == pid || (result < 0 && errno == ECHILD)) return 1;
        usleep(10000);
    }
    return 0;
}

static void stop_child_session(void) {
    input_init(-1);
    if (pty_fd >= 0) {
        close(pty_fd);
        pty_fd = -1;
    }

    const pid_t pid = child_pid;
    child_pid = -1;
    if (pid <= 0 || reap_child(pid, 1)) return;

    kill(-pid, SIGHUP);
    if (reap_child(pid, 20)) return;
    kill(-pid, SIGTERM);
    if (reap_child(pid, 20)) return;
    kill(-pid, SIGKILL);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
}

static int start_child_session(const int argc, char **argv) {
    int master = -1;
    const pid_t pid = spawn_child(&master, argc, argv);
    if (pid < 0 || master < 0) {
        if (master >= 0) close(master);
        return 0;
    }

    const int flags = fcntl(master, F_GETFL);
    if (flags >= 0) fcntl(master, F_SETFL, flags | O_NONBLOCK);

    child_pid = pid;
    pty_fd = master;
    child_exited = 0;
    input_init(pty_fd);
    return 1;
}

static void print_help(const char *program) {
    printf("Mustard Terminal %s\n\n", MUXTERM_VERSION);
    printf("Usage: %s [options] [-- command [arguments...]]\n\n", program);
    printf("  -c, --config <path>       Use a specific terminal config\n");
    printf("  -s, --size <pt>           Set terminal font size\n");
    printf("  -f, --font <path>         Set terminal font\n");
    printf("  --font-hinting <mode>     Set normal, light, mono or none\n");
    printf("  -i, --image <path>        Set terminal background image\n");
    printf("  -bg, --bgcolour <RRGGBB>  Set terminal background colour\n");
    printf("  -fg, --fgcolour <RRGGBB>  Set terminal foreground colour\n");
    printf("  -ro, --readonly           Disable terminal input\n");
    printf("  --non-interactive         Disable all terminal controls\n");
    printf("  --no-sb-persist           Disable scrollback persistence\n");
    printf("  --version                 Print version\n");
}

static void parse_arguments(
    const int argc, char **argv, int *command_index, int *persist_scrollback, int *non_interactive, char *custom_config,
    const size_t size
) {
    int cli_font_size = 0;
    int ignore_muos = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) break;
        if (strcmp(argv[i], "--ignore-muos") == 0) ignore_muos = 1;
        if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc)
            snprintf(custom_config, size, "%s", argv[++i]);
        else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) && i + 1 < argc)
            cli_font_size = atoi(argv[++i]);
    }

    config_load(&term_config, &config, ignore_muos, custom_config[0] ? custom_config : NULL);
    if (cli_font_size > 0) {
        term_config.term_font_size = cli_font_size;
        term_config.term_font_size_explicit = 1;
    }

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            *command_index = i + 1;
            break;
        }
        if ((strcmp(arg, "-c") == 0 || strcmp(arg, "--config") == 0 || strcmp(arg, "-s") == 0
             || strcmp(arg, "--size") == 0)
            && i + 1 < argc) {
            i++;
        } else if ((strcmp(arg, "-f") == 0 || strcmp(arg, "--font") == 0) && i + 1 < argc) {
            snprintf(term_config.term_font_path, sizeof(term_config.term_font_path), "%s", argv[++i]);
            term_config.term_font_path_explicit = 1;
        } else if (strcmp(arg, "--font-italic") == 0 && i + 1 < argc) {
            snprintf(term_config.term_font_path_italic, sizeof(term_config.term_font_path_italic), "%s", argv[++i]);
        } else if (strcmp(arg, "--font-bold") == 0 && i + 1 < argc) {
            snprintf(term_config.term_font_path_bold, sizeof(term_config.term_font_path_bold), "%s", argv[++i]);
        } else if (strcmp(arg, "--font-bold-italic") == 0 && i + 1 < argc) {
            snprintf(
                term_config.term_font_path_bold_italic, sizeof(term_config.term_font_path_bold_italic), "%s", argv[++i]
            );
        } else if (strcmp(arg, "--font-hinting") == 0 && i + 1 < argc) {
            term_config.font_hinting = parse_font_hinting(argv[++i]);
            term_config.font_hinting_explicit = 1;
        } else if ((strcmp(arg, "-i") == 0 || strcmp(arg, "--image") == 0) && i + 1 < argc) {
            snprintf(term_config.bg_image, sizeof(term_config.bg_image), "%s", argv[++i]);
            term_config.background_image_explicit = 1;
        } else if ((strcmp(arg, "-sb") == 0 || strcmp(arg, "--scrollback") == 0) && i + 1 < argc) {
            term_config.scrollback = atoi(argv[++i]);
        } else if (strcmp(arg, "--sb-path") == 0 && i + 1 < argc) {
            snprintf(term_config.scrollback_path, sizeof(term_config.scrollback_path), "%s", argv[++i]);
        } else if (strcmp(arg, "--no-sb-persist") == 0) {
            *persist_scrollback = 0;
        } else if ((strcmp(arg, "-bg") == 0 || strcmp(arg, "--bgcolour") == 0) && i + 1 < argc) {
            if (parse_hex_colour(argv[++i], &term_config.solid_bg)) {
                term_config.use_solid_bg = 1;
                term_config.background_explicit = 1;
            }
        } else if ((strcmp(arg, "-fg") == 0 || strcmp(arg, "--fgcolour") == 0) && i + 1 < argc) {
            if (parse_hex_colour(argv[++i], &term_config.solid_fg)) {
                term_config.use_solid_fg = 1;
                term_config.foreground_explicit = 1;
            }
        } else if (strcmp(arg, "-ro") == 0 || strcmp(arg, "--readonly") == 0) {
            term_config.readonly = 1;
        } else if (strcmp(arg, "--non-interactive") == 0) {
            *non_interactive = 1;
        } else if (strcmp(arg, "--osk-layout") == 0 && i + 1 < argc) {
            snprintf(term_config.osk_layout_path, sizeof(term_config.osk_layout_path), "%s", argv[++i]);
        } else if (strcmp(arg, "--force-redraw") == 0) {
            term_config.force_redraw = 1;
        } else if (strcmp(arg, "--ignore-muos") == 0 || strcmp(arg, "--gl") == 0) {
        } else if (arg[0] != '-') {
            *command_index = i;
            break;
        }
    }
}

static uint64_t navigation_mask(void) {
    return (mux_input_pressed(mux_input_dpad_up) || mux_input_pressed(mux_input_ls_up) ? BIT(0) : 0)
           | (mux_input_pressed(mux_input_dpad_down) || mux_input_pressed(mux_input_ls_down) ? BIT(1) : 0)
           | (mux_input_pressed(mux_input_dpad_left) || mux_input_pressed(mux_input_ls_left) ? BIT(2) : 0)
           | (mux_input_pressed(mux_input_dpad_right) || mux_input_pressed(mux_input_ls_right) ? BIT(3) : 0);
}

static void send_arrow(const int direction) {
    char sequence[3] = {'\x1B', vt_cursor_keys_app() ? 'O' : '[', 'A'};
    if (direction == 1) sequence[2] = 'B';
    if (direction == 2) sequence[2] = 'D';
    if (direction == 3) sequence[2] = 'C';
    input_write(sequence, sizeof(sequence));
}

static void handle_direction(const int direction) {
    if (menu_is_active()) {
        if (direction < 2)
            menu_move(direction == 0 ? -1 : 1);
        else
            menu_adjust(direction == 2 ? -1 : 1);
    } else if (osk_is_visible()) {
        if (direction == 0) osk_move(-1, 0);
        if (direction == 1) osk_move(1, 0);
        if (direction == 2) osk_move(0, -1);
        if (direction == 3) osk_move(0, 1);
    } else {
        send_arrow(direction);
    }
}

static void handle_controller(int *running) {
    static uint64_t previous = 0;
    static uint64_t previous_navigation = 0;
    static int repeat_direction = -1;
    static uint32_t repeat_at = 0;

    const uint64_t navigation = navigation_mask();
    const uint64_t mask = mux_input_pressed_mask();
    const uint64_t edge = mask & ~previous;
    const uint32_t now = SDL_GetTicks();

    if (edge & BIT(mux_input_menu)) {
        if (menu_is_active())
            menu_close();
        else {
            osk_close();
            menu_open();
        }
    }

    if (menu_is_active()) {
        if (edge & BIT(mux_input_a)) menu_select();
        if (edge & BIT(mux_input_b)) menu_close();
        if (edge & BIT(mux_input_x)) menu_reset();
    } else {
        if (edge & BIT(mux_input_select))
            osk_apply_action(
                INPUT_ACT_OSK_TOGGLE, running, &visible_rows, term_config.height, term_config.readonly, pty_fd
            );
        if (edge & (BIT(mux_input_a) | BIT(mux_input_l3)))
            osk_apply_action(INPUT_ACT_PRESS, running, &visible_rows, term_config.height, term_config.readonly, pty_fd);
        if (edge & BIT(mux_input_b))
            osk_apply_action(
                INPUT_ACT_BACKSPACE, running, &visible_rows, term_config.height, term_config.readonly, pty_fd
            );
        if (edge & BIT(mux_input_y))
            osk_apply_action(INPUT_ACT_SPACE, running, &visible_rows, term_config.height, term_config.readonly, pty_fd);
        if (edge & BIT(mux_input_start))
            osk_apply_action(INPUT_ACT_ENTER, running, &visible_rows, term_config.height, term_config.readonly, pty_fd);
        if (edge & BIT(mux_input_l1))
            osk_apply_action(
                INPUT_ACT_LAYER_PREV, running, &visible_rows, term_config.height, term_config.readonly, pty_fd
            );
        if (edge & BIT(mux_input_r1))
            osk_apply_action(
                INPUT_ACT_LAYER_NEXT, running, &visible_rows, term_config.height, term_config.readonly, pty_fd
            );
        if (edge & BIT(mux_input_l2))
            osk_apply_action(
                INPUT_ACT_PAGE_UP, running, &visible_rows, term_config.height, term_config.readonly, pty_fd
            );
        if (edge & BIT(mux_input_r2))
            osk_apply_action(
                INPUT_ACT_PAGE_DOWN, running, &visible_rows, term_config.height, term_config.readonly, pty_fd
            );
    }

    int direction = -1;
    for (int i = 0; i < 4; i++) {
        if (navigation & BIT(i)) {
            direction = i;
            break;
        }
    }

    if (direction >= 0 && !(previous_navigation & BIT(direction))) {
        handle_direction(direction);
        repeat_direction = direction;
        repeat_at = now + (uint32_t) term_config.dpad_repeat_delay;
    } else if (direction >= 0 && repeat_direction == direction && SDL_TICKS_PASSED(now, repeat_at)) {
        handle_direction(direction);
        repeat_at = now + (uint32_t) term_config.dpad_repeat_rate;
    } else if (direction < 0) {
        repeat_direction = -1;
    }

    if (!(mask & (BIT(mux_input_a) | BIT(mux_input_b) | BIT(mux_input_y) | BIT(mux_input_l3)))) osk_hold_end();
    if (osk_hold_tick(now)) {
        if (osk_hold_action() == INPUT_ACT_PRESS) osk_press_key();
        if (osk_hold_action() == INPUT_ACT_BACKSPACE) input_write("\x7F", 1);
        if (osk_hold_action() == INPUT_ACT_SPACE) input_write(" ", 1);
    }

    previous = mask;
    previous_navigation = navigation;
}

static int read_pty(void) {
    struct pollfd descriptor = {pty_fd, POLLIN, 0};
    if (poll(&descriptor, 1, 0) <= 0 || !(descriptor.revents & (POLLIN | POLLHUP))) return 0;

    char batch[65536];
    size_t total = 0;
    ssize_t amount = 0;
    while (total < sizeof(batch) && (amount = read(pty_fd, batch + total, sizeof(batch) - total)) > 0)
        total += (size_t) amount;

    if (total > 0 && vt_feed(batch, total)) vt_scroll_set(0);
    return amount < 0 && errno == EIO;
}

int main(const int argc, char **argv) {
    setlocale(LC_CTYPE, "");

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("Mustard Terminal %s\n", MUXTERM_VERSION);
        return EXIT_SUCCESS;
    }

    int command_index = 0;
    int persist_scrollback = 1;
    int non_interactive = 0;
    char custom_config[PATH_MAX] = "";

    load_device(&device);
    load_config(&config);
    parse_arguments(
        argc, argv, &command_index, &persist_scrollback, &non_interactive, custom_config, sizeof(custom_config)
    );

    init_module("muxterm");
    init_theme(1, 0);
    apply_theme_defaults();
    init_display();
    if (!TTF_WasInit() && TTF_Init() != 0) {
        fprintf(stderr, "Cannot initialise terminal fonts: %s\n", TTF_GetError());
        return EXIT_FAILURE;
    }

    term_config.width = device.mux.width;
    term_config.height = device.mux.height;
    renderer = display_get_renderer();
    if (!renderer) {
        fprintf(stderr, "Cannot initialise terminal renderer: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    if (!reload_terminal_fonts()) {
        fprintf(stderr, "Cannot open terminal font '%s': %s\n", term_config.term_font_path, TTF_GetError());
        return EXIT_FAILURE;
    }

    terminal_columns = term_config.width / cell_width;
    terminal_rows = term_config.height / cell_height;
    if (terminal_columns < 1) terminal_columns = 1;
    if (terminal_rows < 1) terminal_rows = 1;
    visible_rows = terminal_rows;

    if (vt_init(terminal_columns, terminal_rows, term_config.scrollback > 0 ? term_config.scrollback : 1) < 0) {
        fprintf(stderr, "Cannot allocate terminal screen buffer\n");
        return EXIT_FAILURE;
    }

    mkdir("/tmp/mustardos", 0755);
    if (persist_scrollback && term_config.scrollback_path[0]) vt_scrollback_load(term_config.scrollback_path);

    init_ui_common_screen(&theme, &device, &lang, lang.muxterm.title);
    init_fonts();
    set_gradient_visible(0);
    lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_pnl_wall, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_grid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);

    menu_init(&term_config);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    osk_init(ui_screen, term_config.width, term_config.height, cell_height);
    osk_set_repeat(term_config.key_repeat_delay, term_config.key_repeat_rate);
    if (term_config.osk_layout_path[0] && access(term_config.osk_layout_path, R_OK) == 0)
        osk_load_layout(term_config.osk_layout_path);

    terminal_texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, terminal_columns * cell_width,
        terminal_rows * cell_height
    );
    if (!terminal_texture) {
        fprintf(stderr, "Cannot create terminal texture: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    if (term_config.bg_image[0] && access(term_config.bg_image, R_OK) == 0) {
        SDL_Surface *surface = IMG_Load(term_config.bg_image);
        if (surface) {
            background_texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
    }

    char **child_argv = command_index > 0 && command_index < argc ? &argv[command_index] : NULL;
    const int child_argc = child_argv ? argc - command_index : 0;
    if (!start_child_session(child_argc, child_argv)) {
        fprintf(stderr, "Cannot start terminal session: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigchld_handler;
    action.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &action, NULL);
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, NULL);
    action.sa_handler = terminate_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    vt_set_title_callback(on_title_change, NULL);
    if (!non_interactive) {
        board_init(device.board.name);
        mux_input_open();
        SDL_StartTextInput();
    }

    display_set_video_background(draw_terminal);
    display_set_video_background_opaque(1);
    display_set_ui_hidden(0);

    render_screen(
        renderer, terminal_texture, background_texture, term_config.width, visible_rows, term_config.solid_fg,
        term_config.use_solid_fg, term_config.use_solid_bg, term_config.solid_bg, term_config.readonly
    );
    vt_clear_dirty();
    display_composite_frame();
    fade_in_screen();

    int running = 1;
    int shell_dead = 0;
    uint32_t cursor_deadline = 0;
    uint32_t datetime_deadline = 0;
    int previous_osk_state = osk_get_state();
    int previous_osk_visible = osk_is_visible();
    int previous_menu_active = menu_is_active();

    while (running) {
        if (terminate_requested) running = 0;

        if (!non_interactive) {
            input_set_context(term_config.readonly, shell_dead, visible_rows);
            mux_input_poll_raw_unmapped(input_handle_raw);
            handle_controller(&running);
        }

        const int menu_active = menu_is_active();
        const int osk_visible = osk_is_visible();
        if (menu_active != previous_menu_active || osk_visible != previous_osk_visible) {
            previous_menu_active = menu_active;
            previous_osk_visible = osk_visible;
            if (menu_active) {
                menu_show_nav();
            } else if (osk_visible) {
                osk_show_nav();
            } else {
                nav_hide_all();
                lv_obj_set_align(ui_pnl_footer, LV_ALIGN_BOTTOM_MID);
                lv_obj_set_style_bg_color(ui_pnl_footer, lv_color_hex(theme.footer.background), MU_OBJ_MAIN_DEFAULT);
                lv_obj_set_style_bg_opa(ui_pnl_footer, theme.footer.background_alpha, MU_OBJ_MAIN_DEFAULT);
                lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (input_take_quit() || menu_take_quit()) running = 0;
        if (menu_take_reset()) {
            osk_close();
            stop_child_session();
            vt_reset();
            shell_dead = !start_child_session(child_argc, child_argv);
            if (shell_dead) running = 0;
        }
        if (!shell_dead && read_pty()) shell_dead = 1;

        if (child_exited && !shell_dead) {
            int status;
            if (waitpid(child_pid, &status, WNOHANG) > 0) shell_dead = 1;
        }

        const MenuChange change = menu_take_change();
        if (change == MENU_CHANGE_FONT) {
            apply_theme_defaults();
            if (reload_terminal_fonts()) recreate_terminal_texture();
        }
        if (change != MENU_CHANGE_NONE) vt_mark_all_rows_dirty();

        const int osk_state = osk_get_state();
        if (osk_state != previous_osk_state) {
            previous_osk_state = osk_state;
            vt_mark_all_rows_dirty();
        }

        const int opaque_bottom = osk_state == OSK_STATE_BOTTOM_OPAQUE;
        visible_rows =
            opaque_bottom ? (term_config.height - theme.footer.height - osk_get_height()) / cell_height : terminal_rows;
        if (visible_rows < 1) visible_rows = 1;

        const uint32_t now = SDL_GetTicks();
        notify_tick();
        if (SDL_TICKS_PASSED(now, datetime_deadline)) {
            datetime_task(NULL);
            datetime_deadline = now + 1000;
        }
        if (term_config.force_redraw) vt_mark_all_rows_dirty();
        int redraw_terminal = vt_is_dirty() || term_config.force_redraw;
        if (vt_cursor_visible() && !term_config.readonly && !vt_scroll_offset()
            && SDL_TICKS_PASSED(now, cursor_deadline)) {
            vt_mark_cursor_row_dirty();
            redraw_terminal = 1;
            cursor_deadline = now + 500;
        }

        if (redraw_terminal && background_texture) vt_mark_all_rows_dirty();

        if (redraw_terminal) {
            render_screen(
                renderer, terminal_texture, background_texture, term_config.width, visible_rows, term_config.solid_fg,
                term_config.use_solid_fg, term_config.use_solid_bg, term_config.solid_bg, term_config.readonly
            );
            vt_clear_dirty();
        }

        const uint64_t present_before_ui = display_present_serial();
        lv_task_handler();
        lv_refr_now(NULL);
        if (redraw_terminal && display_present_serial() == present_before_ui) display_composite_frame();

        if (shell_dead) running = 0;
        SDL_Delay(redraw_terminal || menu_is_active() || osk_is_visible() ? 16 : 32);
    }

    menu_close();
    if (!non_interactive) {
        SDL_StopTextInput();
        mux_input_close();
    }
    display_clear_video_background();

    if (persist_scrollback && term_config.scrollback_path[0]) vt_scrollback_save(term_config.scrollback_path);
    stop_child_session();

    SDL_DestroyTexture(terminal_texture);
    if (background_texture) SDL_DestroyTexture(background_texture);
    render_glyph_cache_clear();
    for (int i = 0; i < 8; i++)
        if (terminal_fonts[i]) TTF_CloseFont(terminal_fonts[i]);

    osk_free();
    vt_free();
    TTF_Quit();
    sdl_cleanup();
    return EXIT_SUCCESS;
}
