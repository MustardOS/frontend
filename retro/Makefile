include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxretro

CFLAGS  := $(BASE_CFLAGS) -fPIE
LDFLAGS := $(COMMON_LIBS) $(BIN_LDFLAGS) $(LIB_LDFLAGS) -ldl

ifeq ($(DEVICE), NATIVE)
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg -lz -llzma -lbz2
else
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg \
           -Wl,-Bstatic -lz -llzma -Wl,-Bdynamic -lbz2
endif

INCLUDES := -I. -I../module -I../module/ui -I../common \
            -I../common/input -I../common/mini

SRC := main.c core.c environment.c video.c audio.c input_bridge.c state.c \
       ui_pause.c options.c ui_options.c gamestate.c ui_gamestate.c vfs.c rumble.c \
       ui_diskcontrol.c ui_information.c settings.c ui_settings.c patch.c hotkeys.c ui_hotkeys.c \
       content_hash.c

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
