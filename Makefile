# ---- Project ----
TARGET  := doorbell-mqtt-unifi
SRCDIR  := src
INCDIR  := include
OBJDIR  := build
BINDIR  := bin

# Collect sources/objects
SRC     := $(wildcard $(SRCDIR)/*.c)
OBJ     := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))
DEP     := $(OBJ:.o=.d)

# ---- Toolchain ----
CC      := gcc
PKG     := pkg-config
PKGS    := paho-mqtt3c cjson libssh2

# Compiler/Linker flags
WARN    := -Wall -Wextra -Werror -Wpedantic
STD     := -std=c11
GENDEP  := -MMD -MP

# Try pkg-config first; if unavailable, fall back to common defaults.
PKG_CFLAGS := $(shell $(PKG) --cflags --silence-errors $(PKGS))
PKG_LIBS   := $(shell $(PKG) --libs   --silence-errors $(PKGS))

ifeq ($(strip $(PKG_CFLAGS)),)
  $(warning pkg-config not found or some packages missing; using default include paths)
  PKG_CFLAGS :=
endif

ifeq ($(strip $(PKG_LIBS)),)
  $(warning pkg-config not found or some packages missing; using default link flags)
  PKG_LIBS := -lpaho-mqtt3c -lcjson -lssh2
endif

# Common flags (append your include/defines here)
CFLAGS  ?= $(STD) $(WARN) $(GENDEP) -I$(INCDIR) $(PKG_CFLAGS)
LDFLAGS ?=
LDLIBS  ?= $(PKG_LIBS) -lpthread

# ---- Build types ----
# Default is a release-ish build. Use `make debug` for -O0 -g.
OPT_RELEASE := -O2
OPT_DEBUG   := -O0 -g3

# ---- Targets ----
.PHONY: all release debug run clean distclean print
all: release

release: CFLAGS += $(OPT_RELEASE)
release: dirs $(BINDIR)/$(TARGET)

debug: CFLAGS += $(OPT_DEBUG)
debug: dirs $(BINDIR)/$(TARGET)

# Link
$(BINDIR)/$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Create output dirs
dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Run the built binary (after `make`)
run: $(BINDIR)/$(TARGET)
	$(BINDIR)/$(TARGET)

# Housekeeping
clean:
	@$(RM) -r $(OBJDIR)

distclean: clean
	@$(RM) -r $(BINDIR)

# Debug helper
print:
	@echo "SRC:      $(SRC)"
	@echo "OBJ:      $(OBJ)"
	@echo "CFLAGS:   $(CFLAGS)"
	@echo "LDLIBS:   $(LDLIBS)"

# Auto-included dependency files
-include $(DEP)
