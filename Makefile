include common.mk

BIN_DIR = ./bin
LIB_DIR = $(BIN_DIR)/lib

MODULE_DIR = module
MODULES = mubattery mucredits mufbset muhotkey mulog mulookup musplash muwarn muxcharge muxfrontend muxmessage muremap

MODULE_DAEMONS = mudns mulink
INPUT_DAEMON = muinput
DAEMONS = $(MODULE_DAEMONS) $(INPUT_DAEMON)
TOOLS = muvarctl murgb mususpend muverify
CURSOR_LIB = $(LIB_DIR)/libmucursor.so

muvarctl_SRC = common/config/var_store.c

murgb_SRC = common/tooling/rgb_args.c common/config/config.c common/config/config_value.c \
            common/display/colour.c common/storage/fileio_lite.c common/base/strpath.c \
            common/display/theme_base.c common/runtime/log.c common/runtime/debug.c

muverify_LDLIBS = $(EXTERNAL_LIB)/libcrypto.a -ldl -lpthread $(EXTERNAL_HIDE)

DEPENDENCIES = plutosvg common lvgl module

CFLAGS = $(BASE_CFLAGS) $(STRICT_CFLAGS)

INCLUDES = -I. -I./module/ui -I./vendor

LDLIBS = -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

LDFLAGS = $(COMMON_LIBS) $(BIN_LDFLAGS)

DEPDIR := $(DEP_ROOT)/root
$(shell mkdir -p $(DEPDIR))

CONFIG_ID    := $(DEVICE)|$(BUILD)|$(OPT_LEVEL)|$(DEBUGSYM)
CONFIG_STAMP := .build-config
DEP_READY_STAMP := $(DEP_ROOT)/.ready

.PHONY: all $(MODULES) $(DAEMONS) $(TOOLS) darkhttpd cursor prebuild vendor-external generated config-guard clean notify info \
        dep-stage dep-plutosvg dep-lvgl dep-common dep-module dep-retro

.DEFAULT_GOAL := all

all: info prebuild cursor $(MODULES) $(DAEMONS) $(TOOLS) notify

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
	$(VERBOSE)if [ ! -f "$(DEP_READY_STAMP)" ] || [ "$$(cat $(CONFIG_STAMP) 2>/dev/null)" != "$(CONFIG_ID)" ]; then \
		echo "Build dependency state changed, starting clean"; \
		$(MAKE) --no-print-directory clean $(QUIET); \
		mkdir -p "$(DEP_ROOT)"; \
		printf '%s' "$(CONFIG_ID)" >"$(CONFIG_STAMP)"; \
		: >"$(DEP_READY_STAMP)"; \
	fi

generated: | config-guard
	$(VERBOSE)./build.sh generate $(QUIET) || exit 1

dep-stage dep-plutosvg dep-lvgl: | generated
dep-common: dep-plutosvg
dep-module: dep-common dep-lvgl
dep-retro: dep-module

dep-stage:
	@echo "Building Stage Overlay: libmustage.so"
	$(VERBOSE)$(MAKE) -C stage DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

dep-common dep-module:
	@echo "Building Dependency: $(@:dep-%=%)"
	$(VERBOSE)$(MAKE) -C $(@:dep-%=%) DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

dep-plutosvg:
	@echo "Building Dependency: plutosvg"
	$(VERBOSE)$(MAKE) -C vendor/plutosvg DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

dep-lvgl:
	@echo "Building Dependency: lvgl"
	$(VERBOSE)$(MAKE) -C vendor/lvgl DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

dep-retro:
	@echo "Building Libretro Host: muxretro"
	$(VERBOSE)$(MAKE) -C retro DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

prebuild: dep-stage dep-retro

clean:
	$(VERBOSE)rm -rf $(BIN_DIR) $(CONFIG_STAMP) $(DEP_ROOT) vendor/lvgl/build \
		common/generated/language.json common/generated/thirdparty.h
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

$(MODULE_DAEMONS):
	@echo "Building Daemon: $@"
	@mkdir -p $(DEPDIR) $(BIN_DIR)
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) $(MODULE_DIR)/$@.c -o $(BIN_DIR)/$@ \
		-MF $(DEP_ROOT)/root/$@.d $(BIN_LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }

darkhttpd:
	@echo "Building Web Server: $@"
	@mkdir -p $(BIN_DIR)
	$(VERBOSE)$(CC) $(filter-out -D_GNU_SOURCE,$(CFLAGS)) vendor/darkhttpd/darkhttpd.c -o $(BIN_DIR)/$@ \
		$(BIN_LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }

$(INPUT_DAEMON):
	@echo "Building Input Service: $@"
	@mkdir -p $(BIN_DIR)
	$(VERBOSE)$(MAKE) -C input CC="$(CC)" CFLAGS="$(CFLAGS)" \
		LDFLAGS="$(BIN_LDFLAGS)" BUILDDIR="$(abspath $(DEP_ROOT)/input)" $(QUIET) || exit 1
	$(VERBOSE)cp "$(DEP_ROOT)/input/bin/$(INPUT_DAEMON)" "$(BIN_DIR)/$(INPUT_DAEMON)"

$(TOOLS):
	@echo "Building Tool: $@"
	@mkdir -p $(DEPDIR) $(BIN_DIR)
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) $(MODULE_DIR)/$@.c $($@_SRC) -o $(BIN_DIR)/$@ \
		-MF $(DEP_ROOT)/root/$@.d $($@_LDLIBS) $(BIN_LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }

cursor: $(CURSOR_LIB)

$(CURSOR_LIB): common/compat/sdl_cursor.c | prebuild
	@echo "Building SDL Cursor Compatibility Library: $@"
	@mkdir -p $(LIB_DIR)
	$(VERBOSE)$(CC) $(CFLAGS) $(SHARED_PIC) $< -o $@ -ldl $(LIB_LDFLAGS) $(QUIET) || \
		{ echo "Error building $@"; exit 1; }

notify:
	@printf "Compiled %d Modules, %d Daemons and %d Tools\n============== Complete! ==============\n" \
		"$(words $(MODULES))" "$(words $(DAEMONS))" "$(words $(TOOLS))"

-include $(wildcard $(DEPDIR)/*.d)
