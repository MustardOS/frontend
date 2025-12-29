SHELL = /bin/sh

DEVICE := $(strip $(DEVICE))

ifeq ($(DEVICE), ARM64)          # standard aarch64
    ARCH = -march=armv8-a
else ifeq ($(DEVICE), ARM64_A53) # currently for h700 and a133p
    ARCH = -mcpu=cortex-a53+crc+crypto -mtune=cortex-a53 -mfix-cortex-a53-835769 -mfix-cortex-a53-843419
else ifeq ($(DEVICE), ARM32)     # only if you still build armhf for og 35x
    ARCH = -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard
else ifeq ($(DEVICE), ARM32_A9)  # armhf w/thumb for cortex a9 with neon
    ARCH = -mcpu=cortex-a9 -mthumb -mfpu=neon-vfpv3 -mfloat-abi=softfp
else ifeq ($(DEVICE), NATIVE)    # everything else, like maybe x86?
    ARCH = -march=native
else
    $(error Unsupported Device: $(DEVICE))
endif

BIN_DIR = ./bin

MODULE_DIR = module
MODULES = mufbset muhotkey mulookup muterm muxcharge muxcredits muxfrontend muxmessage muxwarn

DEPENDENCIES = common font lvgl lookup module

DEBUG ?= 0
VERBOSE = $(if $(filter 2,$(DEBUG)),, @)
QUIET = $(if $(filter 1,$(DEBUG)),,>/dev/null 2>&1)

CC = ccache $(CROSS_COMPILE)gcc

CFLAGS = $(ARCH) -O3 -pipe -flto=auto \
         -ffunction-sections -fdata-sections \
         -finline-functions -fmerge-all-constants \
         -Wall -Wno-format-zero-length \
         -fno-stack-protector -fno-ident

MUXLIB = $(CFLAGS) -I./module/ui -I./font -I./lookup -I./common \
         -I./common/img -I./common/input -I./common/json \
         -I./common/mini -I./common/miniz

LDFLAGS = $(MUXLIB) -L./bin/lib -lui -llookup -lmux -lmuxmodule \
          -lnotosans_big -lnotosans_big_hd -lnotosans_medium \
          -lnotosans_sc_medium -lnotosans_tc_medium -lnotosans_jp_medium -lnotosans_kr_medium -lnotosans_ar_medium \
          -lSDL2 -lSDL2_mixer -lSDL2_ttf -lSDL2_image -Wl,--gc-sections -s -Wl,-rpath,'./lib'

.PHONY: all $(MODULES) prebuild clean notify

all: info prebuild $(MODULES) clean notify

info:
	@echo "======== muOS Frontend Builder ========"
	@echo "Targeting: $(DEVICE)"
	@echo "Modules: $(MODULES)"
	@echo "Dependencies: $(DEPENDENCIES)"

prebuild:
	$(VERBOSE)for DEP in $(DEPENDENCIES); do \
		echo "Building Dependency: $$DEP"; \
		$(MAKE) -C $$DEP $(QUIET) || { echo "Error building dependency $$DEP"; exit 1; }; \
	done

clean:
	$(VERBOSE)rm -rf .build_count

%.o: $(MODULE_DIR)/%.c
	@echo "Compiling $< to $@"
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) -c $< -o $@ $(LDFLAGS) $(QUIET)

$(MODULES):
	@echo "Building Module: $@"
	$(VERBOSE)UI_FILE="$(MODULE_DIR)/ui/ui_$@.c"; \
	UI_OBJ="$(MODULE_DIR)/ui/ui_$@.o"; \
	if [ -f "$$UI_FILE" ]; then \
		rm -f "$$UI_OBJ"; \
		$(MAKE) -C $(MODULE_DIR)/ui ui_$@.o $(QUIET) || { echo "Error building UI object"; exit 1; }; \
	else \
		UI_OBJ=""; \
	fi; \
	$(CC) -D$(DEVICE) $(MODULE_DIR)/$@.c $$UI_OBJ -o $@ $(LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }; \
	mkdir -p $(BIN_DIR); mv $@ $(BIN_DIR) || { echo "Error moving $@ to $(BIN_DIR)"; exit 1; }
	$(VERBOSE)find ./$(MODULE_DIR) -name "*.o" -exec rm -f {} +

notify:
	@printf "Compiled %d Modules\n============== Complete! ==============\n" "$(words $(MODULES))"
