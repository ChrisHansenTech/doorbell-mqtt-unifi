# AI-Assisted Development Guidelines

This repository maintains an automated C11 test suite (Unity + `gcov`) to protect against regressions across configuration loading, MQTT/Home Assistant integration, SSH/SCP operations, UniFi profile/IPC handling, state validation, and service startup.

All AI coding agents working in this repository **MUST** follow these rules:

## 1. Before Modifying Code

- **Review issue context, docs, and existing tests first**: Before changing any module under `src/` or `include/`, review the associated GitHub issue (when implementing a bug fix or tracked change), relevant project documentation (`TESTING.md`, `docs/`), and the corresponding test suite(s) under `tests/` (`tests/test_*.c`, `tests/integration/`, `tests/smoke/`) to understand requirements, existing behavioral contracts, and mocks.
- **Run relevant baseline tests before editing**: Execute `make test SUITE=<suite>` (or `make test`) before making changes so you know the starting baseline status.

## 2. While Implementing Changes

- **Bug fixes require regression tests**: Every bug fix MUST include a new or expanded test that reproduces the defect (fails before the fix, passes after the fix). Review the associated GitHub issue, relevant documentation (`TESTING.md`, `docs/`), and existing tests to confirm expected behavior before writing the fix.
- **New functionality requires tests**: Every new function, configuration option, MQTT topic/entity, SSH command, error path, or workflow MUST ship with unit tests (and integration/smoke tests where applicable).
- **Consider code coverage impact**: When implementing features or bug fixes, run `make coverage` to verify that new or modified lines, functions, and error branches in `src/*.c` are meaningfully exercised by tests without regressing overall coverage. Never write vacuous tests solely to inflate coverage numbers.
- **Intentional requirement changes**: When requirements intentionally change, update the affected tests and documentation (`TESTING.md`, `docs/`) in the same change and clearly note the behavioral change in your summary.
- **Preserve test isolation**: Tests MUST remain deterministic, offline, fast, and independent. Never require a live UniFi doorbell, live MQTT broker, production credentials, or modifications to tracked repo files or `/config`.
- **Keep production images and builds clean**: Do not add test dependencies or `tests/` into `Dockerfile`, do not link test helpers into `bin/doorbell-mqtt-unifi`, and never enable `--coverage` instrumentation in normal `release` or `debug` builds.

## 3. Integrity Rules (Non-Negotiable)

- **NEVER remove, skip, ignore, or weaken tests** solely to make an implementation pass.
- **NEVER alter assertions to codify a known bug as intended behavior.** If an existing defect blocks a test, document it clearly rather than asserting broken behavior.
- **NEVER claim tests passed unless you actually executed them successfully** after your final edit and observed zero failures.
- **Always report failures and unverified areas**: Explicitly report any failed tests, skipped/ignored tests, compiler warnings, or hardware-dependent functionality that could not be verified automatically.

## 4. Before Reporting Completion

Run the full applicable validation suite after your final edit:

```bash
make lint
make test
make coverage
make release
```

Verify that:
- All test suites pass with zero failures and zero ignored tests.
- Coverage changes for modified `src/*.c` files (line, function, and branch coverage) are reviewed and reported.
- Strict C11 compilation (`-Wall -Wextra -Werror -Wpedantic`) succeeds for both test binaries and the release binary without coverage instrumentation in the release binary.
- No temporary files, mutation experiments, or generated coverage artifacts (`coverage/`, `build-cov/`, `bin-cov/`, `*.gcda`, `*.gcno`) are left untracked in git.
