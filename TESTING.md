# Testing Guide

This document describes the automated testing architecture, conventions, commands, coverage reporting, and hardware-validation boundaries for `doorbell-mqtt-unifi`.

## Testing Architecture

`doorbell-mqtt-unifi` is a C11 service with no web UI or HTTP API. It bridges Home Assistant (over MQTT via `libpaho-mqtt3c`) with a UniFi Protect G4 Doorbell Pro (over SSH/SCP and remote shell/`ubnt_ipc_cli`/`playSound.sh` commands via `libssh2`).

Automated tests are organized into three layers:

1. **Unit Tests (`tests/test_*.c`)** — Test individual C modules in isolation. External library and transport boundaries (`libpaho-mqtt3c`, `libssh2`, TCP sockets, `mqtt_*`, `ssh_*`) are intercepted at link time via GCC's `-Wl,--wrap=<symbol>` mechanism so real application logic runs deterministically without external services or production code changes.
2. **Integration Tests (`tests/integration/test_*.c`)** — Exercise full multi-module workflows end-to-end (config loading → profile repo → HA topics/discovery/bind → `mqtt_router` worker thread → `command_*` → `unifi_remote` → `unifi_ipc` → `state` persistence & SHA-256 validation → `ha_status` MQTT publishes) against an in-memory simulated doorbell state.
3. **Deployment & Smoke Tests (`tests/smoke/test_*.c`)** — Execute the compiled `doorbell-mqtt-unifi` binary in isolated child processes with temporary directories and environment variables to validate startup failure handling, configuration loading, banner output, and clean `SIGTERM` shutdown.

All automated tests run completely offline: no physical UniFi doorbell, live UniFi Protect controller, MQTT broker, or SSH credentials are required.

## Testing Frameworks and Dependencies

- **Test framework**: [Unity](tests/third_party/unity/) (embedded in the repository; no external package installation required). Compiled with `-DUNITY_INCLUDE_DOUBLE`.
- **Compiler & linker**: `gcc` (C11, `-Wall -Wextra -Werror -Wpedantic`) with GNU `ld` `--wrap` support.
- **Coverage tools**: `gcov` (bundled with `gcc`) and `python3` (standard library only, via `scripts/coverage_report.py`).
- **Build dependencies** (same as native application build):
  - `build-essential`
  - `pkg-config`
  - `libssl-dev`
  - `libssh2-1-dev`
  - `libpaho-mqtt-dev`
- **Optional static analysis**: `cppcheck` (automatically run by `make lint` if installed).

Install dependencies on Debian/Ubuntu/Raspberry Pi OS:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  pkg-config \
  libssl-dev \
  libssh2-1-dev \
  libpaho-mqtt-dev \
  python3
```

Testing files (`tests/`, `scripts/`) are **not** copied into the production `Dockerfile` image and are not linked into the production `doorbell-mqtt-unifi` binary.

## Directory Structure

```text
tests/
├── fixtures/                   # Static test fixtures (config JSONs, sample profiles, invalid assets)
│   ├── config/
│   ├── invalid_assets/
│   └── profiles/
├── integration/                # Multi-module end-to-end integration test suites
│   └── test_integration_workflows.c
├── mocks/                      # Per-suite link-time --wrap mocks (linked only into suites that need them)
│   ├── mock_libssh2.{c,h}      # Wraps libssh2_* + TCP socket calls + nanosleep
│   ├── mock_mqtt.{c,h}         # Wraps mqtt_publish/subscribe/lwt/callbacks
│   ├── mock_paho.{c,h}         # Wraps Paho MQTTClient_* + time + nanosleep + mqtt_router_enqueue
│   └── mock_ssh.{c,h}          # Wraps ssh_session_*/ssh_exec_command/ssh_scp_*
├── smoke/                      # Subprocess startup and shutdown smoke tests
│   └── test_smoke_startup.c
├── support/                    # Shared non-wrapping test utilities linked into all test binaries
│   ├── test_config_mock.{c,h}
│   └── test_helpers.{c,h}      # Isolated /tmp dir creation/cleanup & minimal PNG/OGG/WAV generators
├── third_party/unity/          # Embedded Unity test framework
└── test_*.c                    # Unit test suites (one compiled binary per file in bin/)
scripts/
└── coverage_report.py          # Zero-dependency gcov line, function, and branch coverage aggregator (terminal, LCOV, HTML, JSON)
```

## Test Naming Conventions

- **Unit test files**: `tests/test_<module>.c` → builds to `bin/test_<module>`.
- **Integration test files**: `tests/integration/test_integration_<area>.c` → builds to `bin/test_integration_<area>`.
- **Smoke test files**: `tests/smoke/test_smoke_<area>.c` → builds to `bin/test_smoke_<area>`.
- **Test functions**: `void test_<component>_<behavior_or_condition>(void)` using descriptive snake_case names.
- **Main entry point**: Every test binary MUST return `UNITY_END()` (`return UNITY_END();`) so test failures propagate non-zero exit codes to `make` and CI.

## Running Tests

### Run the entire automated test suite

```bash
make test
```

### Run specific test groups

```bash
make test-unit          # Unit tests only
make test-integration   # Integration tests only
make test-smoke         # Startup & deployment smoke tests only
```

### Run a single test suite

```bash
make test SUITE=test_config
# or build and invoke directly:
make test-bin
./bin/test_config
```

### Run linting, static analysis, tests, and release build together

```bash
make lint               # Shell syntax check + strict C11 warnings (+ cppcheck if installed)
make check              # Runs lint, test, and release build
```

### Clean environment run

```bash
make distclean && make check
```

## Generating Coverage Reports

Run the complete coverage workflow with a single command:

```bash
make coverage
```

This uses the dedicated `BUILD=coverage` build configuration, which is completely isolated from normal `release` and `debug` builds (`build/` and `bin/` are never instrumented with `--coverage`).

What `make coverage` does:
1. Clears all previous coverage data and generated reports (`make coverage-clean`, removing `build-cov/`, `bin-cov/`, and `coverage/`).
2. Builds all application sources and test binaries with `BUILD=coverage` (`-O0 -g3 --coverage`) into `build-cov/` and `bin-cov/`.
3. Executes the complete automated test suite (unit, integration, and smoke tests, running `bin-cov/doorbell-mqtt-unifi` for smoke tests so `src/main.c` is measured).
4. Runs `scripts/coverage_report.py` to generate:
   - **Terminal summary**: Per-file and overall **line**, **function**, and **branch** coverage.
   - **LCOV report**: `coverage/lcov.info` (standard LCOV tracefile with `FN`/`FNDA`, `DA`, and `BRDA` records).
   - **Detailed HTML report**: `coverage/html/index.html` (summary dashboard + per-file annotated source pages with line hit counts, function call counts, and inline branch indicators).
   - **JSON report**: `coverage/coverage.json` (machine-readable per-file metrics, uncovered lines, and uncovered functions).

Additional coverage targets:

```bash
make coverage-build     # Compile coverage-instrumented binaries only (build-cov/, bin-cov/)
make coverage-clean     # Remove build-cov/, bin-cov/, and coverage/
```

All first-party `src/*.c` files (including any file with 0% test coverage) are included in coverage calculations. Test code, mocks, headers, and third-party libraries (`src/third_party/`) are excluded.

## Visual Studio Code Integration

### Native "Run with Coverage" Support Assessment

The repository's existing VS Code test integration (`.vscode/settings.json`) uses the `unityExplorer.*` configuration for Makefile-built Unity C binaries. Neither the `unityExplorer` extension nor the C/Makefile toolchain implements VS Code's native `TestRunProfileKind.Coverage` ("Run with Coverage") API.

Instead, full in-editor coverage visualization is configured via the **Coverage Gutters** extension (`ryanluker.vscode-coverage-gutters`) using the LCOV report at `coverage/lcov.info`, paired with dedicated VS Code tasks.

### Recommended Extensions

Open the workspace in VS Code and accept the recommended extensions from `.vscode/extensions.json`:
- **Coverage Gutters** (`ryanluker.vscode-coverage-gutters`) — Displays line/gutter/ruler coverage overlays directly in C source files and previews the HTML report.
- **C/C++** (`ms-vscode.cpptools`) — IntelliSense and GDB debugging.
- **Makefile Tools** (`ms-vscode.makefile-tools`) — Makefile build integration.

### VS Code Tasks

Run tasks via **Terminal → Run Task…** (`Ctrl+Shift+P` → `Tasks: Run Task`):
- **`Test (with coverage)`** — Runs `make coverage` to clear previous data, execute all tests, print the terminal summary, and regenerate `coverage/lcov.info`, `coverage/html/index.html`, and `coverage/coverage.json`.
- **`Build (coverage)`** — Runs `make coverage-build`.
- **`Coverage: Open HTML Report`** — Runs `Test (with coverage)` and opens `coverage/html/index.html` in the default browser.
- **`Coverage: Clean`** — Runs `make coverage-clean`.
- **`Lint & Check`** — Runs `make check`.

### Viewing Coverage in VS Code

1. Run the **`Test (with coverage)`** task (or `make coverage` in the integrated terminal).
2. Open any file under `src/` (for example, `src/config.c` or `src/unifi_remote.c`).
3. Click **`Watch`** in the VS Code status bar (provided by Coverage Gutters) or run **`Coverage Gutters: Display Coverage`** (`Ctrl+Shift+P`) to highlight covered (green) and uncovered (red) lines in the editor gutter and overview ruler.
4. To view the detailed HTML report, run **`Coverage: Open HTML Report`**, run **`Coverage Gutters: Preview Coverage Report`**, or open `coverage/html/index.html` in a browser.

## Mocking and Fixture Conventions

- **Prefer real code over mocks**: Use real filesystem operations in isolated temporary directories (`/tmp/dmu-test-XXXXXX`) created via `test_make_temp_dir()` and cleaned up in `tearDown()` via `test_remove_dir_recursive()`.
- **External boundary mocking**: Use GCC `-Wl,--wrap=<symbol>` with mocks under `tests/mocks/`. Never modify production `.c` files solely to swap out dependencies unless a clear architectural seam is justified.
- **Environment isolation**: Any test that reads or modifies environment variables (`MQTT_*`, `SSH_*`, `UNIFI_*`, `SFX_*`, `LOG_LEVEL`, `TEST_SSH_PASS`) must clear/reset those variables in both `setUp()` and `tearDown()`.
- **Working directory isolation**: Tests exercising code with relative paths (`./profiles`, `./test-profile`) must `chdir()` into an isolated temporary directory in `setUp()` and restore the original working directory in `tearDown()`.
- **No arbitrary sleeps**: Synchronize asynchronous worker tests with condition variables or deterministic shutdown (`mqtt_router_stop()` drains queued messages before joining) and wrap `time`/`nanosleep` for backoff tests.

## Adding New Tests

1. **Adding to an existing suite**: Add a `test_<behavior>()` function to the relevant `tests/test_<module>.c` and register it with `RUN_TEST(...)` in `main()`.
2. **Adding a new standard unit suite**: Create `tests/test_<name>.c`. The `Makefile` automatically discovers `tests/test_*.c` via wildcard and links it against `CORE_OBJS`, Unity, and `tests/support/*.c`.
3. **Adding a suite that requires `--wrap` mocks**: Add a dedicated link rule in `Makefile` referencing the required `MOCK_*_OBJ` and `WRAP_*_FLAGS` (following `$(BINDIR)/test_ha` or `$(BINDIR)/test_command`).
4. **Adding fixtures**: Place small, deterministic fixtures under `tests/fixtures/` or generate minimal valid PNG/OGG/WAV files in `/tmp` using `test_helpers.h`. Never rely on untracked files (`.env`, `config.json`, `profiles/downloads/`, `sounds/`).

## Optional Hardware-Dependent Integration Testing

The following behaviors require a real UniFi Protect G4 Doorbell Pro, an MQTT broker, and Home Assistant and are **excluded** from automated CI tests:

- Live TCP/SSH handshake and password authentication against the doorbell's Dropbear/OpenSSH daemon using `UNIFI_PROTECT_RECOVERY_CODE`.
- Actual execution of `ubnt_ipc_cli` (`ChangeLcmGuiSettings`, `ChangeSoundLedSettings`) and verification of firmware JSON response schemas across UniFi Protect firmware versions.
- Actual execution of `/usr/local/bin/playSound.sh` and audible playback on doorbell hardware.
- Legacy mode service restart behavior (`killall ubnt_lcm_gui ubnt_sounds_leds` and `pidof` recovery polling).
- Storage guardrail shell script execution against `/var/etc/persistent` (`df -kP` / `du -sk` under BusyBox).
- Home Assistant MQTT Discovery ingestion, entity rendering, and UI interaction in a live Home Assistant instance.
- Multi-architecture Docker container runtime behavior (`docker-entrypoint.sh` `PUID`/`PGID` `gosu` privilege drop and volume permissions).

See [docs/hardware-testing.md](docs/hardware-testing.md) for the complete manual hardware validation procedures, safety rules, permanent expected behavior, and temporary limitations.

## Known Limitations

- **Static global state in C modules**: Modules such as `src/mqtt.c` (`connected`, `last_attempt`), `src/mqtt_router.c` (`routes[]`), and `src/ha_status.c` (`g_last_error_*`) retain process-wide static state. Because each test suite compiles into its own binary, suites are isolated from each other; within a suite, `setUp()`/`tearDown()` must reset state carefully.
- **Unreachable/defensive OOM and cross-filesystem branches**: Allocation failure (`malloc`/`calloc`/`realloc`/`strdup`/`cJSON_Create*` returning `NULL`) and filesystem `errno == EXDEV` cross-device rename paths are not fault-injected in the unit suite.
- **VS Code test explorer scope**: `.vscode/settings.json` configures `unityExplorer.testSourceFileRegex` as `tests\/test_.*\.c`, which discovers unit tests only (`tests/test_*.c`). Integration (`tests/integration/`) and smoke (`tests/smoke/`) suites are executed via `make test`, `make coverage`, or the workspace VS Code tasks.
- **Offline transport mocking**: Automated suites intercept `libpaho-mqtt3c`, `libssh2`, and POSIX socket calls at link time. Live doorbell firmware behavior, Home Assistant UI rendering, and Docker container startup must be validated manually per [docs/hardware-testing.md](docs/hardware-testing.md).
- **Open defects not codified as passing behavior**: Automated tests do not assert broken behavior for unresolved defects. Remote upload, staging, and apply error propagation in `src/unifi_remote.c` and `src/command.c` are verified by unit regression tests (`tests/test_unifi_remote.c` and `tests/test_command.c`); any remaining unresolved behaviors are tracked in the repository's GitHub issues and summarized under temporary limitations in [docs/hardware-testing.md](docs/hardware-testing.md).

## Coverage Exclusions

`scripts/coverage_report.py` measures only first-party `src/*.c` files and excludes:

| Excluded Path | Justification |
|---|---|
| `src/third_party/cJSON/cJSON.c` | Vendored upstream third-party JSON library |
| `src/third_party/crypto/md5.c` | Vendored upstream third-party MD5 implementation |
| `tests/**` | Test harness, mocks, helpers, and embedded Unity framework |
| System/header files (`/usr/include/**`) | Standard library and external SDK headers |

All 25 first-party files under `src/*.c` (including `src/main.c`) are included in coverage reporting with no first-party files excluded.
