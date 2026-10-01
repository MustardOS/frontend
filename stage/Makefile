include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(LIB_DIR)/libmustage.so

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -fPIC
LDFLAGS := -shared -Wl,--strip-all $(STAGE_LDFLAGS) -ldl -lSDL2 -lSDL2_image -lGLESv2 -lEGL -lm -lpthread

RUNTIME_SYMS := ^(gl|egl|vk|drm|SDL_|IMG_|Mix_)
INCLUDES := -I. -I.. -I./common -I./overlay -I./render

SRC := hook.c common/common.c ../common/runtime/debug.c ../common/storage/inotify.c ../common/runtime/log.c \
	common/alpha.c common/anchor.c common/colour.c common/filter.c common/overlay.c \
	common/rotate.c common/scale.c common/stretch.c common/shader.c \
	overlay/base.c overlay/battery.c overlay/bright.c overlay/volume.c \
	render/sdl.c render/gles.c render/kms.c render/vulkan.c

DEPDIR := $(DEP_ROOT)/stage
OBJDIR := $(DEPDIR)/obj
stage_obj = $(OBJDIR)/$(subst /,_,$(subst ../,up_/,$(1:.c=.o)))
OBJ := $(foreach S,$(SRC),$(call stage_obj,$(S)))

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

DEPS := $(foreach S,$(SRC),$(call DEP_NAME,stage,$(basename $(S))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

define compile_source
$(call stage_obj,$(1)): $(1)
	@echo "Compiling $$<"
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $$(INCLUDES) -c $$< -o $$@ -MF $$(call DEP_NAME,stage,$$(basename $(1)))
endef

$(foreach S,$(SRC),$(eval $(call compile_source,$(S))))

clean:
	rm -rf $(DEPDIR)
