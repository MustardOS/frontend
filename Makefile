include common.mk

BIN_DIR = ./bin
LIB_DIR = $(BIN_DIR)/lib

MODULE_DIR = module
MODULES = mufbset muhotkey mulookup muterm muxcharge muxcredits muxfrontend muxmessage muxwarn

DEPENDENCIES = common font lvgl lookup module

CFLAGS = $(BASE_CFLAGS)

INCLUDES = -I./module/ui -I./font -I./lookup -I./common \
           -I./common/img -I./common/input -I./common/json \
           -I./common/mini -I./common/miniz

LDLIBS = -L$(LIB_DIR) -lui -llookup -lmux -lmuxmodule \
         -lnotosans_big -lnotosans_big_hd -lnotosans_medium \
         -lnotosans_sc_medium -lnotosans_tc_medium -lnotosans_jp_medium \
         -lnotosans_kr_medium -lnotosans_ar_medium

LDFLAGS = $(COMMON_LIBS) $(BIN_LDFLAGS) $(LIB_LDFLAGS)

.PHONY: all $(MODULES) prebuild clean notify info

all: info prebuild $(MODULES) clean notify

info:
	@echo "======== muOS Frontend Builder ========"
	@echo "Targeting: $(DEVICE)"
	@echo "Modules: $(MODULES)"
	@echo "Dependencies: $(DEPENDENCIES)"

prebuild:
	$(VERBOSE)for DEP in $(DEPENDENCIES); do \
		echo "Building Dependency: $$DEP"; \
		$(MAKE) -C $$DEP DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || { echo "Error building dependency $$DEP"; exit 1; }; \
	done

clean:
	$(VERBOSE)rm -rf .build_count

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
	$(VERBOSE)find ./$(MODULE_DIR) -name "*.o" -exec rm -f {} +

notify:
	@printf "Compiled %d Modules\n============== Complete! ==============\n" "$(words $(MODULES))"
