# ===== Project =====
TARGET  := doorbell-mqtt-unifi

SRCDIR  := src
INCDIR  := include
OBJDIR  := build
BINDIR  := bin

# ---- Sources ----
# Your project sources + embedded cJSON source
CJSON_SRC := $(firstword \
  $(wildcard $(SRCDIR)/third_party/cJSON/cJSON.c) \
)
ifeq ($(strip $(CJSON_SRC)),)
  $(error Could not find cJSON.c (expected at src/third_party/cJSON/cJSON.c))
endif

SRCS := \
  $(wildcard $(SRCDIR)/*.c) \
  $(CJSON_SRC)

# Map sources to objects in build tree (preserve subdirs)
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ===== Toolchain / Flags =====
CC    := gcc
PKG   := pkg-config

# Per-package flags with fallbacks so a missing .pc doesn't kill the whole link
PAHO_CFLAGS := $(shell $(PKG) --cflags paho-mqtt3c 2>/dev/null)
PAHO_LIBS   := $(shell $(PKG) --libs   paho-mqtt3c 2>/dev/null)
ifeq ($(strip $(PAHO_LIBS)),)
  $(warning pkg-config missing for paho-mqtt3c; falling back to -lpaho-mqtt3c)
  PAHO_LIBS := -lpaho-mqtt3c
endif

SSH2_CFLAGS := $(shell $(PKG) --cflags libssh2 2>/dev/null)
SSH2_LIBS   := $(shell $(PKG) --libs   libssh2 2>/dev/null)
ifeq ($(strip $(SSH2_LIBS)),)
  $(warning pkg-config missing for libssh2; falling back to -lssh2)
  SSH2_LIBS := -lssh2
endif

# Include paths (add cJSON header path)
INCLUDES := -I$(INCDIR) -I$(INCDIR)/third_party/cJSON

# Common compiler/linker flags
CSTD   := -std=c11
WARN   := -Wall -Wextra -Werror -Wpedantic
GENDEP := -MMD -MP

# Build type toggles
OPT_RELEASE := -O2 -DNDEBUG
OPT_DEBUG   := -O0 -g3 -DDEBUG

# Default (release) unless BUILD=debug or target 'debug' is used
CFLAGS  ?= $(CSTD) $(WARN) $(GENDEP) $(INCLUDES) $(PAHO_CFLAGS) $(SSH2_CFLAGS)
LDFLAGS ?=
LDLIBS  ?= $(PAHO_LIBS) $(SSH2_LIBS) -lpthread

# ===== Phony targets =====
.PHONY: all release debug run clean distclean print dirs

all: release

release: CFLAGS += $(OPT_RELEASE)
release: dirs $(BINDIR)/$(TARGET)

debug: CFLAGS += $(OPT_DEBUG)
debug: dirs $(BINDIR)/$(TARGET)

# Allow `make BUILD=debug` too
ifeq ($(BUILD),debug)
CFLAGS += $(OPT_DEBUG)
endif

# ===== Link =====
$(BINDIR)/$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

# ===== Compile (preserve subdirs under build/, create dep files) =====
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ===== Dirs =====
dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Convenience
run: $(BINDIR)/$(TARGET)
	$(BINDIR)/$(TARGET)

clean:
	@$(RM) -r $(OBJDIR)

distclean: clean
	@$(RM) -r $(BINDIR)

print:
	@echo "TARGET:  $(TARGET)"
	@echo "SRCS:    $(SRCS)"
	@echo "OBJS:    $(OBJS)"
	@echo "CFLAGS:  $(CFLAGS)"
	@echo "LDLIBS:  $(LDLIBS)"

# ===== Auto-included dependency files =====
-include $(DEPS)
