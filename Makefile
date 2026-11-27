# ===== Project =====
TARGET  := doorbell-mqtt-unifi

SRCDIR  := src
INCDIR  := include
COVDIR  := coverage

ifeq ($(BUILD),coverage)
OBJDIR  ?= build-cov
BINDIR  ?= bin-cov
else
OBJDIR  ?= build
BINDIR  ?= bin
endif

# ---- Sources ----
# Your project sources + embedded cJSON source
CJSON_SRC := $(firstword \
  $(wildcard $(SRCDIR)/third_party/cJSON/cJSON.c) \
)
ifeq ($(strip $(CJSON_SRC)),)
  $(error Could not find cJSON.c (expected at src/third_party/cJSON/cJSON.c))
endif

MD5_SRC := $(firstword \
  $(wildcard $(SRCDIR)/third_party/crypto/md5.c) \
)
ifeq ($(strip $(MD5_SRC)),)
  $(error Could not find md5.c (expected at src/third_party/crypto/md5.c))
endif

SRCS := \
  $(wildcard $(SRCDIR)/*.c) \
  $(CJSON_SRC) \
  $(MD5_SRC)

# Map sources to objects in build tree (preserve subdirs)
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ===== Test objects (exclude main.c so we don't get two mains) =====
MAIN_SRC  := $(SRCDIR)/main.c
CORE_SRCS := $(filter-out $(MAIN_SRC),$(SRCS))
CORE_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(CORE_SRCS))

# Unity test framework
UNITY_DIR   := tests/third_party/unity

# ===== Tests: one binary per test suite =====
UNITY_SRC := $(UNITY_DIR)/unity.c
UNITY_OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(UNITY_SRC))

UNIT_TEST_SRCS        := $(sort $(wildcard tests/test_*.c))
INTEGRATION_TEST_SRCS := $(sort $(wildcard tests/integration/test_*.c))
SMOKE_TEST_SRCS       := $(sort $(wildcard tests/smoke/test_*.c))

UNIT_TEST_OBJS        := $(patsubst %.c,$(OBJDIR)/%.o,$(UNIT_TEST_SRCS))
INTEGRATION_TEST_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(INTEGRATION_TEST_SRCS))
SMOKE_TEST_OBJS       := $(patsubst %.c,$(OBJDIR)/%.o,$(SMOKE_TEST_SRCS))

UNIT_TEST_BINS        := $(patsubst tests/test_%.c,$(BINDIR)/test_%,$(UNIT_TEST_SRCS))
INTEGRATION_TEST_BINS := $(patsubst tests/integration/test_%.c,$(BINDIR)/test_%,$(INTEGRATION_TEST_SRCS))
SMOKE_TEST_BINS       := $(patsubst tests/smoke/test_%.c,$(BINDIR)/test_%,$(SMOKE_TEST_SRCS))

TEST_SUITE_OBJS := $(UNIT_TEST_OBJS) $(INTEGRATION_TEST_OBJS) $(SMOKE_TEST_OBJS)
TEST_BINS       := $(UNIT_TEST_BINS) $(INTEGRATION_TEST_BINS) $(SMOKE_TEST_BINS)

# Shared non-wrapping test support code (config mocks, temp dir helpers, fixture builders)
TEST_SUPPORT_SRCS := $(sort $(wildcard tests/support/*.c))
TEST_SUPPORT_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(TEST_SUPPORT_SRCS))

# Per-suite mock objects (linked only into suites that use --wrap)
MOCK_SRCS := $(sort $(wildcard tests/mocks/*.c))
MOCK_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(MOCK_SRCS))

MOCK_MQTT_OBJ    := $(OBJDIR)/tests/mocks/mock_mqtt.o
MOCK_SSH_OBJ     := $(OBJDIR)/tests/mocks/mock_ssh.o
MOCK_PAHO_OBJ    := $(OBJDIR)/tests/mocks/mock_paho.o
MOCK_LIBSSH2_OBJ := $(OBJDIR)/tests/mocks/mock_libssh2.o

# Test objects need Unity + support + mocks include paths
$(UNITY_OBJ) $(TEST_SUITE_OBJS) $(TEST_SUPPORT_OBJS) $(MOCK_OBJS): CFLAGS += -DUNITY_INCLUDE_DOUBLE -Itests -I$(UNITY_DIR) -Itests/support -Itests/mocks

# ===== Linker --wrap groups =====
WRAP_MQTT_FLAGS := \
  -Wl,--wrap=mqtt_publish \
  -Wl,--wrap=mqtt_subscribe \
  -Wl,--wrap=mqtt_set_last_will \
  -Wl,--wrap=mqtt_set_on_connect \
  -Wl,--wrap=mqtt_set_on_disconnect

WRAP_MQTT_PUB_FLAGS := \
  -Wl,--wrap=mqtt_publish

WRAP_SSH_OPS_FLAGS := \
  -Wl,--wrap=ssh_exec_command \
  -Wl,--wrap=ssh_scp_upload_file \
  -Wl,--wrap=ssh_scp_download_file

WRAP_SSH_ALL_FLAGS := \
  -Wl,--wrap=ssh_session_create \
  -Wl,--wrap=ssh_session_destroy \
  $(WRAP_SSH_OPS_FLAGS)

WRAP_PAHO_FLAGS := \
  -Wl,--wrap=MQTTClient_create \
  -Wl,--wrap=MQTTClient_setCallbacks \
  -Wl,--wrap=MQTTClient_connect \
  -Wl,--wrap=MQTTClient_subscribe \
  -Wl,--wrap=MQTTClient_publishMessage \
  -Wl,--wrap=MQTTClient_disconnect \
  -Wl,--wrap=MQTTClient_destroy \
  -Wl,--wrap=MQTTClient_freeMessage \
  -Wl,--wrap=MQTTClient_free \
  -Wl,--wrap=time \
  -Wl,--wrap=nanosleep \
  -Wl,--wrap=mqtt_router_enqueue

WRAP_LIBSSH2_FLAGS := \
  -Wl,--wrap=libssh2_init \
  -Wl,--wrap=libssh2_exit \
  -Wl,--wrap=getaddrinfo \
  -Wl,--wrap=freeaddrinfo \
  -Wl,--wrap=socket \
  -Wl,--wrap=connect \
  -Wl,--wrap=close \
  -Wl,--wrap=libssh2_session_init_ex \
  -Wl,--wrap=libssh2_session_set_blocking \
  -Wl,--wrap=libssh2_session_handshake \
  -Wl,--wrap=libssh2_userauth_password_ex \
  -Wl,--wrap=libssh2_session_disconnect_ex \
  -Wl,--wrap=libssh2_session_free \
  -Wl,--wrap=libssh2_channel_open_ex \
  -Wl,--wrap=libssh2_channel_process_startup \
  -Wl,--wrap=libssh2_channel_read_ex \
  -Wl,--wrap=libssh2_channel_write_ex \
  -Wl,--wrap=libssh2_channel_eof \
  -Wl,--wrap=libssh2_channel_close \
  -Wl,--wrap=libssh2_channel_wait_closed \
  -Wl,--wrap=libssh2_channel_get_exit_status \
  -Wl,--wrap=libssh2_channel_free \
  -Wl,--wrap=libssh2_channel_send_eof \
  -Wl,--wrap=libssh2_channel_wait_eof \
  -Wl,--wrap=libssh2_scp_send64 \
  -Wl,--wrap=libssh2_scp_recv2 \
  -Wl,--wrap=nanosleep

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
INCLUDES := -I$(INCDIR) -I$(INCDIR)/third_party/cJSON -I$(INCDIR)/third_party/crypto

# Common compiler/linker flags
CSTD   := -std=c11
WARN   := -Wall -Wextra -Werror -Wpedantic
GENDEP := -MMD -MP

# Build type toggles
OPT_RELEASE  := -O2 -DNDEBUG -D_POSIX_C_SOURCE=200809L
OPT_DEBUG    := -O0 -g3 -fno-omit-frame-pointer -fno-inline -DDEBUG -D_POSIX_C_SOURCE=200809L
OPT_COVERAGE := $(OPT_DEBUG) --coverage

# Default (release) unless BUILD=debug/coverage or target 'debug'/'coverage' is used
CFLAGS  ?= $(CSTD) $(WARN) $(GENDEP) $(INCLUDES) $(PAHO_CFLAGS) $(SSH2_CFLAGS)
# Allow CI/Docker to append flags without clobbering defaults
CFLAGS  += $(EXTRA_CFLAGS)
LDFLAGS ?=
LDFLAGS += $(EXTRA_LDFLAGS)
LDLIBS  ?= $(PAHO_LIBS) $(SSH2_LIBS) -lpthread -lcrypto

# ===== Phony targets =====
.PHONY: all release debug run clean distclean print dirs \
        test test-bin test-unit test-integration test-smoke \
        coverage coverage-build coverage-clean lint check

all: release

release: CFLAGS += $(OPT_RELEASE)
release: dirs $(BINDIR)/$(TARGET)

debug: CFLAGS += $(OPT_DEBUG)
debug: dirs $(BINDIR)/$(TARGET)

# Allow `make BUILD=debug` and `make BUILD=coverage` too
ifeq ($(BUILD),debug)
CFLAGS += $(OPT_DEBUG)
endif
ifeq ($(BUILD),coverage)
CFLAGS  += $(OPT_COVERAGE)
LDFLAGS += --coverage
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

# ===== Tests =====
# Use `make test` to run all suites, or `make test SUITE=test_config` for one suite.
test-bin: CFLAGS += $(OPT_DEBUG)
test-bin: dirs $(BINDIR)/$(TARGET) $(TEST_BINS)
	@true

test: test-bin
	@set -e; \
	if [ -n "$(SUITE)" ]; then \
		echo "==> $(BINDIR)/$(SUITE)"; \
		DOORBELL_TEST_BIN="$(BINDIR)/$(TARGET)" "$(BINDIR)/$(SUITE)"; \
	else \
		for t in $(TEST_BINS); do \
			echo "==> $$t"; \
			DOORBELL_TEST_BIN="$(BINDIR)/$(TARGET)" "$$t"; \
		done; \
	fi

test-unit: CFLAGS += $(OPT_DEBUG)
test-unit: dirs $(UNIT_TEST_BINS)
	@set -e; \
	for t in $(UNIT_TEST_BINS); do \
		echo "==> $$t"; \
		"$$t"; \
	done

test-integration: CFLAGS += $(OPT_DEBUG)
test-integration: dirs $(INTEGRATION_TEST_BINS)
	@set -e; \
	for t in $(INTEGRATION_TEST_BINS); do \
		echo "==> $$t"; \
		"$$t"; \
	done

test-smoke: CFLAGS += $(OPT_DEBUG)
test-smoke: dirs $(BINDIR)/$(TARGET) $(SMOKE_TEST_BINS)
	@set -e; \
	for t in $(SMOKE_TEST_BINS); do \
		echo "==> $$t"; \
		DOORBELL_TEST_BIN="$(BINDIR)/$(TARGET)" "$$t"; \
	done

# Specialized test binaries with --wrap mocks
$(BINDIR)/test_mqtt: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_PAHO_OBJ) $(OBJDIR)/tests/test_mqtt.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(WRAP_PAHO_FLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_PAHO_OBJ) $(OBJDIR)/tests/test_mqtt.o -o $@ $(LDLIBS)

$(BINDIR)/test_ssh: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_LIBSSH2_OBJ) $(OBJDIR)/tests/test_ssh.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(WRAP_LIBSSH2_FLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_LIBSSH2_OBJ) $(OBJDIR)/tests/test_ssh.o -o $@ $(LDLIBS)

$(BINDIR)/test_ha: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_MQTT_OBJ) $(OBJDIR)/tests/test_ha.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(WRAP_MQTT_FLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_MQTT_OBJ) $(OBJDIR)/tests/test_ha.o -o $@ $(LDLIBS)

$(BINDIR)/test_unifi_remote: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_SSH_OBJ) $(OBJDIR)/tests/test_unifi_remote.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(WRAP_SSH_OPS_FLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_SSH_OBJ) $(OBJDIR)/tests/test_unifi_remote.o -o $@ $(LDLIBS)

$(BINDIR)/test_command: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_SSH_OBJ) $(MOCK_MQTT_OBJ) $(OBJDIR)/tests/test_command.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(WRAP_SSH_ALL_FLAGS) $(WRAP_MQTT_PUB_FLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_SSH_OBJ) $(MOCK_MQTT_OBJ) $(OBJDIR)/tests/test_command.o -o $@ $(LDLIBS)

$(BINDIR)/test_integration_%: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_SSH_OBJ) $(MOCK_MQTT_OBJ) $(OBJDIR)/tests/integration/test_integration_%.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(WRAP_SSH_ALL_FLAGS) $(WRAP_MQTT_FLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(MOCK_SSH_OBJ) $(MOCK_MQTT_OBJ) $(OBJDIR)/tests/integration/test_integration_$*.o -o $@ $(LDLIBS)

$(BINDIR)/test_smoke_%: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(OBJDIR)/tests/smoke/test_smoke_%.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(OBJDIR)/tests/smoke/test_smoke_$*.o -o $@ $(LDLIBS)

# Generic rule for standard unit test binaries (no mocks needed)
$(BINDIR)/test_%: $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(OBJDIR)/tests/test_%.o | $(BINDIR)
	$(CC) $(LDFLAGS) $(CORE_OBJS) $(UNITY_OBJ) $(TEST_SUPPORT_OBJS) $(OBJDIR)/tests/test_$*.o -o $@ $(LDLIBS)

# ===== Coverage =====
coverage-clean:
	@$(RM) -r build-cov bin-cov $(COVDIR)

coverage-build:
	$(MAKE) test-bin BUILD=coverage EXTRA_CFLAGS="$(EXTRA_CFLAGS)" EXTRA_LDFLAGS="$(EXTRA_LDFLAGS)"

coverage: coverage-clean
	$(MAKE) test BUILD=coverage EXTRA_CFLAGS="$(EXTRA_CFLAGS)" EXTRA_LDFLAGS="$(EXTRA_LDFLAGS)"
	python3 scripts/coverage_report.py build-cov \
		--lcov $(COVDIR)/lcov.info \
		--html $(COVDIR)/html \
		--json $(COVDIR)/coverage.json

# ===== Lint / Static Analysis =====
lint:
	@echo "==> Checking shell script syntax"
	sh -n docker-entrypoint.sh
	@echo "==> Checking strict C11 compilation warnings on first-party sources"
	@set -e; \
	for f in $(wildcard $(SRCDIR)/*.c); do \
		$(CC) $(CSTD) $(WARN) -Wformat=2 -Wundef $(OPT_RELEASE) $(INCLUDES) $(PAHO_CFLAGS) $(SSH2_CFLAGS) -fsyntax-only "$$f"; \
	done
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "==> Running cppcheck"; \
		cppcheck --enable=warning,performance,portability --error-exitcode=1 --std=c11 -I$(INCDIR) $(wildcard $(SRCDIR)/*.c); \
	else \
		echo "==> cppcheck not installed (optional); compiler syntax checks passed"; \
	fi

check: lint test release

clean:
	@$(RM) -r $(OBJDIR) build-cov $(COVDIR)

distclean: clean
	@$(RM) -r $(BINDIR) bin-cov

print:
	@echo "TARGET:  $(TARGET)"
	@echo "SRCS:    $(SRCS)"
	@echo "OBJS:    $(OBJS)"
	@echo "CFLAGS:  $(CFLAGS)"
	@echo "LDLIBS:  $(LDLIBS)"

# ===== Auto-included dependency files =====
-include $(DEPS)
