include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxretro

CFLAGS  := $(BASE_CFLAGS) -fPIE
LDFLAGS := $(COMMON_LIBS) $(BIN_LDFLAGS) $(LIB_LDFLAGS) -ldl -Wl,--allow-shlib-undefined

ifeq ($(DEVICE), NATIVE)
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg -lz -llzma -lbz2
else
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg \
           -Wl,-Bstatic -lz -llzma -Wl,-Bdynamic -lbz2
endif

INCLUDES := -I../module -I../module/ui -I../common \
            -I../common/input -I../common/mini

SRC := core/main.c core/core.c core/environment.c core/runahead.c \
       video/video.c video/colour.c video/overlay_bridge.c video/frame_pacer.c \
       video/ui_display.c video/ui_colfilter.c video/ui_shader.c video/ui_viewport.c \
       video/ui_videosettings.c video/hw_render.c \
       video/filters/filters.c video/filters/scale2x.c video/filters/super_eagle.c \
       audio/audio.c audio/ui_soundsettings.c \
       input/input_bridge.c input/rumble.c input/hotkeys.c input/ui_hotkeys.c \
       input/ui_inputsettings.c input/nav_repeat.c \
       state/state.c state/gamestate.c state/ui_gamestate.c state/vfs.c \
       state/content_hash.c state/bios_check.c state/sram.c \
       state/patch.c state/ui_storagesettings.c state/manual.c \
       settings/settings.c settings/submenu.c settings/ui_settings.c \
       settings/ui_performancesettings.c settings/ui_hudsettings.c \
       ui/ui_pause.c ui/options.c ui/ui_options.c ui/ui_information.c \
       ui/ui_diskcontrol.c ui/cheats.c ui/ui_cheats.c ui/ui_patch.c ui/ui_manual.c

OBJ := $(SRC:.c=.o)

LA_DIR     := ../common/libarchive
LA_SRC     := $(wildcard $(LA_DIR)/*.c)
LA_OBJ     := $(LA_SRC:.c=.o)
LA_CFLAGS  := $(BASE_CFLAGS) -DHAVE_CONFIG_H -DLIBARCHIVE_STATIC -I$(LA_DIR)

.PHONY: all clean

all: $(TARGET) clean

$(TARGET): $(OBJ) $(LA_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) $(LA_OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LA_DIR)/%.o: $(LA_DIR)/%.c
	$(CC) $(LA_CFLAGS) -c $< -o $@

clean: | $(TARGET)
	rm -f $(OBJ) $(LA_OBJ)
