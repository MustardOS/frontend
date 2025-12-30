include ../common.mk

CFLAGS = $(BASE_CFLAGS)
LDFLAGS = $(COMMON_LIBS) $(SHARED_PIC)

BUILD_DIR      = ./build
BUILD_OBJ_DIR  = $(BUILD_DIR)/obj
BUILD_BIN_DIR  = ../bin/lib

LVGL_DIR_NAME ?= .
LVGL_DIR      ?= .

include $(LVGL_DIR)/$(LVGL_DIR_NAME)/lvgl.mk

OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
FOBJS = $(AOBJS) $(COBJS)

TARGET = $(addprefix $(BUILD_OBJ_DIR)/, $(patsubst ./%, %, $(FOBJS)))

LIBRARY = $(BUILD_BIN_DIR)/libui.so

all: $(LIBRARY)

$(BUILD_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(BUILD_OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(LIBRARY): $(TARGET)
	@mkdir -p $(dir $(LIBRARY))
	$(CC) -o $@ $(TARGET) $(LDFLAGS)
	@echo "Built shared library: $@"
	@rm -rf $(BUILD_DIR)
