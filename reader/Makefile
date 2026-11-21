include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/module/muxreader

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -I. -I.. -I../module -I../module/ui -I../vendor
LDFLAGS := $(COMMON_LIBS) $(MUPDF_LIBS) $(EXTERNAL_HIDE) $(BIN_LDFLAGS)
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

SRC := main.c menu.c settings.c view.c engine/document.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $(TARGET))
	$(CC) $(OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

DEPDIR := $(DEP_ROOT)/reader
DEPS   := $(foreach O,$(OBJ),$(call DEP_NAME,reader,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,reader,$*)

clean:
	rm -f $(OBJ)
	rm -rf $(DEPDIR)
