include ../common.mk

CFLAGS   = $(BASE_CFLAGS) $(STRICT_CFLAGS) -I.. -I../vendor \
           -I../vendor/plutosvg/source -I../vendor/plutosvg/plutovg/include \
           -isystem ../vendor/miniz
LDFLAGS  = $(COMMON_LIBS) $(COMMON_EXTERNAL_LDFLAGS) $(LIB_LDFLAGS) $(SHARED_PIC) -L../bin/lib -lplutosvg

BASE_SRCS = \
    base/randname.c base/strpath.c base/strutil.c base/totp.c base/util.c

RUNTIME_SRCS = \
    runtime/debug.c runtime/exec.c runtime/init.c runtime/log.c runtime/perf.c \
    runtime/task_exec.c runtime/task_protocol.c

CONFIG_SRCS = \
    config/config.c config/config_value.c config/ini.c config/kiosk.c \
    config/passcode.c config/skip.c config/var_store.c

PLATFORM_SRCS = \
    platform/audio.c platform/battery.c platform/bluetooth.c platform/board.c \
    platform/control.c platform/device.c platform/display.c platform/input.c \
    platform/soundfont.c platform/sysinfo.c platform/timezone.c platform/video.c

STORAGE_SRCS = \
    storage/archive.c storage/download.c storage/fileio.c storage/fileio_lite.c \
    storage/inotify.c storage/union.c storage/verify.c

CONTENT_SRCS = \
    content/catalogue.c content/content.c content/lookup.c \
    content/core/common.c content/core/external.c content/core/retroarch.c \
    content/collection/common.c content/collection/order.c \
    content/collection/tag.c content/collection/theme.c

DISPLAY_SRCS = \
    display/anim.c display/colour.c display/datetime.c display/image.c \
    display/language.c display/overlay.c display/screenshot.c display/svg.c \
    display/text.c display/theme.c display/theme_base.c

UI_SRCS = \
    ui/cache.c ui/common.c ui/dialogue.c ui/empty_state.c ui/font.c \
    ui/fs_choice.c ui/glyph.c ui/grid.c ui/image.c ui/list_frame.c \
    ui/modal.c ui/more.c ui/nav.c ui/notify.c ui/orientation.c ui/osk.c \
    ui/task_progress.c ui/task_prompt.c ui/transition.c \
    ui/navigation/grid_nav.c ui/navigation/list_nav.c

SAVER_SRCS = \
    saver/saver.c saver/blockfall.c saver/boxart.c saver/bsod.c saver/constellation.c \
    saver/datetime.c saver/dvd.c saver/firefly.c saver/matrix.c saver/maze.c \
    saver/mystify.c saver/pulse.c saver/slideshow.c saver/star.c saver/trace.c

TOOLING_SRCS = \
    tooling/fbset_args.c tooling/lookup_args.c tooling/message_args.c tooling/rgb_args.c

COMPAT_SRCS = compat/sdl_cursor.c

VENDOR_SRCS = \
    ../vendor/json/json.c ../vendor/mini/mini.c ../vendor/miniz/miniz.c \
    ../vendor/xxhash/xxhash.c

ifeq ($(BUILD),release)
    CRASH_SRC = runtime/crash.c
else
    CRASH_SRC = runtime/crash_stub.c
endif

SRCS = \
    $(BASE_SRCS) $(RUNTIME_SRCS) $(CRASH_SRC) $(CONFIG_SRCS) \
    $(PLATFORM_SRCS) $(STORAGE_SRCS) $(CONTENT_SRCS) $(DISPLAY_SRCS) \
    $(UI_SRCS) $(SAVER_SRCS) $(TOOLING_SRCS) $(COMPAT_SRCS) $(VENDOR_SRCS)
OBJS     = $(SRCS:.c=.o)

../vendor/miniz/miniz.o: CFLAGS += -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

LIB_DIR  = ${shell pwd}/../bin/lib
LIB_FILE = libmuxcom.so

all: $(LIB_DIR)/$(LIB_FILE)

$(LIB_DIR)/$(LIB_FILE): $(OBJS)
	@printf "Linking objects into %s\n" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(OBJS) $(LDFLAGS) -o $@

DEPDIR := $(DEP_ROOT)/common
DEPS   := $(foreach O,$(OBJS),$(call DEP_NAME,common,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS) -MF $(call DEP_NAME,common,$*)
