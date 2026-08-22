include common.mk

BIN_DIR = ./bin
LIB_DIR = $(BIN_DIR)/lib

MODULE_DIR = module
MODULES = mubattery mucredits mufbset muhotkey mulog mulookup musplash muwarn muxcharge muxfrontend muxmessage muremap

DAEMONS = mulink
TOOLS = muvarctl murgb

muvarctl_SRC = common/var_store.c

murgb_SRC = common/rgb_args.c common/config.c common/config_value.c common/colour.c \
            common/fileio_lite.c common/strpath.c common/theme_base.c common/log.c common/debug.c

DEPENDENCIES = plutosvg common lvgl module

CFLAGS = $(BASE_CFLAGS) $(STRICT_CFLAGS)

INCLUDES = -I./module/ui -I./common \
           -I./common/input -I./common/json \
           -I./common/mini -I./common/miniz

LDLIBS = -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

LDFLAGS = $(COMMON_LIBS) $(BIN_LDFLAGS)

DEPDIR := $(DEP_ROOT)/root
$(shell mkdir -p $(DEPDIR))

CONFIG_ID    := $(DEVICE)|$(BUILD)|$(OPT_LEVEL)|$(DEBUGSYM)
CONFIG_STAMP := .build-config

.PHONY: all $(MODULES) $(DAEMONS) $(TOOLS) prebuild vendor-external thirdparty config-guard clean notify info \
        dep-stage dep-plutosvg dep-lvgl dep-common dep-module dep-retro

.DEFAULT_GOAL := all

all: info prebuild $(MODULES) $(DAEMONS) $(TOOLS) notify

$(MODULES) $(DAEMONS) $(TOOLS): | prebuild
notify: | $(MODULES) $(DAEMONS) $(TOOLS)

info:
	@echo "======== MustardOS Frontend Builder ========"
	@echo "Targeting: $(DEVICE)"
	@echo "Modules: $(MODULES)"
	@echo "Daemons: $(DAEMONS)"
	@echo "Tools: $(TOOLS)"
	@echo "Dependencies: $(DEPENDENCIES)"

vendor-external:
	@echo "Building External Dependencies"
	$(VERBOSE)DEVICE="$(DEVICE)" EXT_ARCH_FLAGS="$(ARCH)" $(EXTERNAL_BUILD) $(QUIET) || exit 1

config-guard: | vendor-external
	$(VERBOSE)if [ "$$(cat $(CONFIG_STAMP) 2>/dev/null)" != "$(CONFIG_ID)" ]; then \
		echo "Build configuration changed, starting clean"; \
		$(MAKE) --no-print-directory clean $(QUIET); \
		printf '%s' "$(CONFIG_ID)" > $(CONFIG_STAMP); \
	fi

thirdparty: | config-guard
	$(VERBOSE)./gen_thirdparty.sh $(QUIET) || exit 1

dep-stage dep-plutosvg dep-lvgl: | thirdparty
dep-common: dep-plutosvg
dep-module: dep-common dep-lvgl
dep-retro: dep-module

dep-stage:
	@echo "Building Stage Overlay: libmustage.so"
	$(VERBOSE)$(MAKE) -C stage DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

dep-plutosvg dep-lvgl dep-common dep-module:
	@echo "Building Dependency: $(@:dep-%=%)"
	$(VERBOSE)$(MAKE) -C $(@:dep-%=%) DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

dep-retro:
	@echo "Building Libretro Host: muxretro"
	$(VERBOSE)$(MAKE) -C retro DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

prebuild: dep-stage dep-retro

clean:
	$(VERBOSE)rm -rf $(BIN_DIR) $(CONFIG_STAMP) $(DEP_ROOT) lvgl/build common/thirdparty.h
	$(VERBOSE)find . \( -name "*.o" -o -name "*.d" \) \
		-not -path "./.git/*" -not -path "./external/*" -exec rm -f {} +


%.o: $(MODULE_DIR)/%.c
	@echo "Compiling $< to $@"
	@mkdir -p $(DEPDIR)
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MF $(call DEP_NAME,root,$*) $(QUIET)

$(MODULES):
	@echo "Building Module: $@"
	@mkdir -p $(DEPDIR)
	$(VERBOSE)UI_FILE="$(MODULE_DIR)/ui/ui_$@.c"; \
	UI_OBJ="$(MODULE_DIR)/ui/ui_$@.o"; \
	if [ -f "$$UI_FILE" ]; then \
		rm -f "$$UI_OBJ"; \
		$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) -c "$$UI_FILE" -o "$$UI_OBJ" \
			-MF $(DEP_ROOT)/root/ui_$@.d $(QUIET) || { echo "Error building UI object"; exit 1; }; \
	else \
		UI_OBJ=""; \
	fi; \
	$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) $(MODULE_DIR)/$@.c $$UI_OBJ -o $@ \
		-MF $(DEP_ROOT)/root/$@.d $(LDLIBS) $(LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }; \
	mkdir -p $(BIN_DIR); mv $@ $(BIN_DIR) || { echo "Error moving $@ to $(BIN_DIR)"; exit 1; }

$(DAEMONS):
	@echo "Building Daemon: $@"
	@mkdir -p $(DEPDIR) $(BIN_DIR)
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(MODULE_DIR)/$@.c -o $(BIN_DIR)/$@ \
		-MF $(DEP_ROOT)/root/$@.d $(BIN_LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }

$(TOOLS):
	@echo "Building Tool: $@"
	@mkdir -p $(DEPDIR) $(BIN_DIR)
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(MODULE_DIR)/$@.c $($@_SRC) -o $(BIN_DIR)/$@ \
		-MF $(DEP_ROOT)/root/$@.d $(BIN_LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }

notify:
	@printf "Compiled %d Modules, %d Daemons and %d Tools\n============== Complete! ==============\n" \
		"$(words $(MODULES))" "$(words $(DAEMONS))" "$(words $(TOOLS))"

-include $(wildcard $(DEPDIR)/*.d)
