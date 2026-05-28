include common.mk

BIN_DIR = ./bin
LIB_DIR = $(BIN_DIR)/lib

MODULE_DIR = module
MODULES = mubattery mucredits mufbset muhotkey mulog mulookup murgb musplash muwarn muxcharge muxfrontend muxmessage muremap

DEPENDENCIES = plutosvg common lvgl lookup module

CFLAGS = $(BASE_CFLAGS)

INCLUDES = -I./module/ui -I./lookup -I./common \
           -I./common/input -I./common/json \
           -I./common/mini -I./common/miniz

LDLIBS = -L$(LIB_DIR) -lui -llookup -lmuxcom -lmuxmod -lplutosvg

LDFLAGS = $(COMMON_LIBS) $(BIN_LDFLAGS) $(LIB_LDFLAGS)

.PHONY: all $(MODULES) prebuild clean notify info

all: info prebuild $(MODULES) clean notify

$(MODULES): | prebuild
clean notify: | $(MODULES)

info:
	@echo "======== muOS Frontend Builder ========"
	@echo "Targeting: $(DEVICE)"
	@echo "Modules: $(MODULES)"
	@echo "Dependencies: $(DEPENDENCIES)"

prebuild:
	$(VERBOSE)rm -rf $(BIN_DIR)
	$(VERBOSE)find . -name "*.o" -not -path "./.git/*" -exec rm -f {} +
	@echo "Building Stage Overlay: libmustage.so"
	$(VERBOSE)$(MAKE) -C stage DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1
	$(VERBOSE)for DEP in $(DEPENDENCIES); do \
		echo "Building Dependency: $$DEP"; \
		$(MAKE) -C $$DEP DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1; \
	done

clean:
	$(VERBOSE)rm -rf .build_count
	$(VERBOSE)find ./$(MODULE_DIR) -name "*.o" -exec rm -f {} +


%.o: $(MODULE_DIR)/%.c
	@echo "Compiling $< to $@"
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) -c $< -o $@ $(QUIET)

$(MODULES):
	@echo "Building Module: $@"
	$(VERBOSE)UI_FILE="$(MODULE_DIR)/ui/ui_$@.c"; \
	UI_OBJ="$(MODULE_DIR)/ui/ui_$@.o"; \
	if [ -f "$$UI_FILE" ]; then \
		rm -f "$$UI_OBJ"; \
		$(MAKE) -C $(MODULE_DIR)/ui DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" ui_$@.o $(QUIET) || { echo "Error building UI object"; exit 1; }; \
	else \
		UI_OBJ=""; \
	fi; \
	$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) $(MODULE_DIR)/$@.c $$UI_OBJ -o $@ $(LDLIBS) $(LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }; \
	mkdir -p $(BIN_DIR); mv $@ $(BIN_DIR) || { echo "Error moving $@ to $(BIN_DIR)"; exit 1; }

notify:
	@printf "Compiled %d Modules\n============== Complete! ==============\n" "$(words $(MODULES))"
