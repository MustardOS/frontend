include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxretro

CFLAGS  := $(BASE_CFLAGS) -fPIE
LDFLAGS := $(COMMON_LIBS) $(BIN_LDFLAGS) $(LIB_LDFLAGS) -ldl
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

INCLUDES := -I. -I../module -I../module/ui -I../common \
            -I../common/input -I../common/mini

SRC := main.c core.c environment.c video.c audio.c input_bridge.c state.c \
       ui_pause.c options.c ui_options.c gamestate.c ui_gamestate.c vfs.c rumble.c \
       ui_diskcontrol.c ui_information.c settings.c ui_settings.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET) clean

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean: | $(TARGET)
	rm -f $(OBJ)
