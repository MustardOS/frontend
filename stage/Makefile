include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(LIB_DIR)/libmustage.so

CFLAGS  := $(BASE_CFLAGS) -fPIC
LDFLAGS := -shared -Wl,--strip-all -ldl -lSDL2 -lSDL2_image -lGLESv2

INCLUDES := -I. -I./common -I./overlay -I./render -I../common

SRC := hook.c common/common.c ../common/inotify.c \
	common/alpha.c common/anchor.c common/scale.c \
	overlay/base.c overlay/battery.c overlay/bright.c overlay/volume.c \
	render/sdl.c render/gles.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET) clean

$(TARGET): $(OBJ)
	@echo "Building Stage Overlay: $@"
	@mkdir -p $(LIB_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ)
