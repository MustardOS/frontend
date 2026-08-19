include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(LIB_DIR)/libmustage.so

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -fPIC
LDFLAGS := -shared -Wl,--strip-all $(STAGE_LDFLAGS) -ldl -lSDL2 -lSDL2_image -lGLESv2 -lEGL -lm -lpthread

RUNTIME_SYMS := ^(gl|egl|vk|drm|SDL_|IMG_|Mix_)
INCLUDES := -I. -I./common -I./overlay -I./render -I../common

SRC := hook.c common/common.c ../common/debug.c ../common/inotify.c ../common/log.c \
	common/alpha.c common/anchor.c common/colour.c common/filter.c common/overlay.c \
	common/rotate.c common/scale.c common/stretch.c common/shader.c \
	overlay/base.c overlay/battery.c overlay/bright.c overlay/volume.c \
	render/sdl.c render/gles.c render/kms.c render/vulkan.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "Building Stage Overlay: $@"
	@mkdir -p $(LIB_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	@MISSING=$$($(NM) -D --undefined-only $@ \
		| awk '$$1 == "U" && $$2 !~ /@GLIBC/ && $$2 !~ /$(RUNTIME_SYMS)/ { print $$2 }'); \
	if [ -n "$$MISSING" ]; then \
		echo "Undefined symbols in $@ that nothing will provide at runtime:"; \
		echo "$$MISSING" | sed 's/^/  /'; \
		rm -f $@; \
		exit 1; \
	fi

DEPDIR := $(DEP_ROOT)/stage
DEPS   := $(foreach O,$(OBJ),$(call DEP_NAME,stage,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MF $(call DEP_NAME,stage,$*)

clean:
	rm -f $(OBJ)
	rm -rf $(DEPDIR)
