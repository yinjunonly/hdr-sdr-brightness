# v1.1.5 Windows Night Light Follow Hotfix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce and locally validate a v1.1.5 candidate that follows the actual Windows Night Light state for every Windows scheduling mode and refreshes within 15 seconds.

**Architecture:** Keep Windows scheduling policy in Windows. The app consumes only the current readable Night Light active state, falls back to its internal schedule when that state is unavailable, and invalidates the active-state cache on each 15-second recheck. Release preflight runs the full non-clipboard verification and enforces test-suite wiring before either formal package script produces artifacts.

**Tech Stack:** Win32 C++17, Windows registry/CloudStore, PowerShell, MinGW-w64, MSIX.

## Global Constraints

- Base all release work on clean tag `v1.1.4`; do not import unrelated dirty-worktree changes.
- Follow Windows means follow the current Windows Night Light active state for sunset/sunrise, custom hours, and manual toggles.
- Missed registry notifications must recover within 15 seconds.
- An unreadable Windows state must fall back to the built-in app schedule.
- Do not commit, push, publish, submit, or message users in this implementation run.
- Live testing may use Windows Settings UI only and must restore the original configuration.

---

### Task 1: Restore periodic active-state refresh

**Files:**
- Test: `app_tests/night_mode_cache_test.cpp`
- Test: `app_tests/periodic_night_refresh_contract_test.ps1`
- Modify: `src/night_mode.h`
- Modify: `src/night_mode.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `void night_mode::InvalidateActiveStateCache()`
- Consumes: the existing `kRecheckTimer` branch in `MainWndProc`

- [ ] **Step 1: Run the existing cache suite and timer contract for RED**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\app_tests\build.ps1
```

Expected: compilation fails because `InvalidateActiveStateCache` is absent, or
the timer contract fails because the call is absent.

- [ ] **Step 2: Restore the minimal API**

Declare `InvalidateActiveStateCache()` in `night_mode.h`; implement it by
setting only `g_nightLightActiveCacheValid = false`.

- [ ] **Step 3: Restore the timer call**

In `MainWndProc`, call `night_mode::InvalidateActiveStateCache()` immediately
before `ApplyCurrentBrightness(false)` in the `kRecheckTimer` branch.

- [ ] **Step 4: Verify GREEN**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\app_tests\build.ps1
```

Expected: the existing active-state cache refresh assertion and timer contract
both pass.

### Task 2: Follow the actual Windows active state

**Files:**
- Modify: `app_tests/night_mode_cache_test.cpp`
- Modify: `src/night_mode.cpp`

**Interfaces:**
- Consumes: `night_mode::Schedule`, `night_mode::Decide`
- Produces: `night_mode::Decide(const Schedule&)` behavior independent of Windows schedule type

- [ ] **Step 1: Add one failing Follow Windows behavior with schedule variants**

Extend the test process stub with configurable settings JSON and use an internal
schedule with equal start times so its deterministic fallback is day. In one
table-driven behavior, assert that an active Windows Night Light state selects
night for sunset/sunrise, custom hours, and scheduling-disabled/manual-on
settings. Assert that Windows is the decision source.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\app_tests\build.ps1
```

Expected: sunset/sunrise passes, while custom hours or scheduling-disabled
manual-on reports that the app selected the built-in day schedule.

- [ ] **Step 3: Implement the minimal active-state decision**

Change `CanFollowWindowsNightLight()` to return whether
`GetNightLightActiveCached()` is known. Change `Decide` so Follow Windows reads
that active state directly and returns it when known; otherwise retain
`IsFixedNightNow(schedule)`.

- [ ] **Step 4: Re-run the focused test and verify GREEN**

Run the same `app_tests\build.ps1` command and require all schedule variants to
pass.

### Task 3: Cover inactive state and unreadable fallback

**Files:**
- Modify: `app_tests/night_mode_cache_test.cpp`

**Interfaces:**
- Consumes: the public `night_mode::Decide` result and decision source
- Produces: regression coverage for inactive and unreadable Windows states

- [ ] **Step 1: Add inactive-state coverage**

For each Windows settings JSON variant, expose a readable inactive state and
assert day with `DecisionSourceWindowsNightLight`.

- [ ] **Step 2: Add unreadable-state fallback coverage**

Make both registry and cloud-reader active-state reads fail, use equal internal
start times, and assert the result is day from
`DecisionSourceFixedSchedule`.

- [ ] **Step 3: Run the complete focused suite**

```powershell
powershell -ExecutionPolicy Bypass -File .\app_tests\build.ps1
```

Expected: all app-state assertions and the timer contract pass.

### Task 4: Restore verification wiring and add release preflight

**Files:**
- Modify: `verify.ps1`
- Create: `release-preflight.ps1`
- Modify: `package.ps1`
- Modify: `package-msix.ps1`

**Interfaces:**
- Produces: `release-preflight.ps1 -AllowDirtySource`
- Consumes: `verify.ps1 -SkipClipboardTests`

- [ ] **Step 1: Add a failing preflight wiring check**

`release-preflight.ps1` must inspect `verify.ps1` for
`app_tests\build.ps1` and inspect both package scripts for
`release-preflight.ps1`. It must fail on the current source before production
logic changes.

- [ ] **Step 2: Verify RED**

```powershell
powershell -ExecutionPolicy Bypass -File .\release-preflight.ps1 -AllowDirtySource -WiringOnly
```

Expected: failure because the app-state suite and package preflight calls are
not wired.

- [ ] **Step 3: Restore and enforce the wiring**

Add `app_tests\build.ps1` to `verify.ps1`. Add preflight calls to both package
scripts before their build steps. The preflight must reject tracked-source
changes unless `-AllowDirtySource` is explicit, and must run
`verify.ps1 -SkipClipboardTests` unless `-WiringOnly` is selected.

- [ ] **Step 4: Verify GREEN for the wiring contract**

Run the Step 2 command again and require exit code 0.

### Task 5: Prepare v1.1.5 local candidate

**Files:**
- Modify: `VERSION`
- Create: `store/release-notes-1.1.5.md`

**Interfaces:**
- Produces: version `1.1.5` for desktop executable and manifest version `1.1.5.0`

- [ ] **Step 1: Change the version**

Set `VERSION` to `1.1.5`.

- [ ] **Step 2: Add concise release notes**

State that running instances now follow Windows Night Light changes, including
custom hours and manual controls, with a maximum 15-second fallback check.

- [ ] **Step 3: Validate version wiring**

Run isolated desktop and Store builds and inspect executable/manifest versions.

### Task 6: Run automated release verification

**Files:**
- No source changes expected

- [ ] **Step 1: Run focused app-state tests**

```powershell
powershell -ExecutionPolicy Bypass -File .\app_tests\build.ps1
```

- [ ] **Step 2: Run full non-clipboard verification**

```powershell
powershell -ExecutionPolicy Bypass -File .\verify.ps1 -SkipClipboardTests
```

- [ ] **Step 3: Run diff and source checks**

Review `git diff --check`, the exact changed-file list, and the diff against
`v1.1.4`; reject any unrelated product changes.

### Task 7: Package and install the local Store candidate

**Files:**
- Generated: `dist/HdrSdrBrightness-1.1.5-win64.msix`
- Generated: `dist/HdrSdrBrightness-1.1.5-win64.msixupload`

- [ ] **Step 1: Build local packages through preflight**

```powershell
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.1.5 -Clean -AllowDirtySource
```

- [ ] **Step 2: Validate package identity and payload**

Inspect the manifest version, architecture, publisher/signature, executable
file version, and SHA256 hashes.

- [ ] **Step 3: Install in place**

Install the developer-signed MSIX without uninstalling the existing package or
deleting application data. Confirm package `Status=Ok`, version `1.1.5.0`, and
that the running process path is inside the installed WindowsApps package.

### Task 8: Perform live Windows Settings validation

**Files:**
- Generated diagnostic screenshots/logs under `obj/validation/night-light-live`

- [ ] **Step 1: Record the original state**

Capture Windows Night Light enabled state, schedule mode/times, app day/night
levels, current SDR brightness, package identity, and running process path.

- [ ] **Step 2: Test manual enable and disable**

Use Windows Settings UI to enable Night Light and require night brightness
within 15 seconds; disable it and require day brightness within 15 seconds.

- [ ] **Step 3: Test a custom scheduled transition**

Through Windows Settings UI, select custom hours with the start one minute
ahead, wait for Windows to activate Night Light, then require the app's night
brightness within 15 seconds.

- [ ] **Step 4: Restore original settings**

Restore the exact original Windows schedule mode, times, and enabled state.
Re-read the settings and current SDR brightness to prove restoration.

### Task 9: Preserve the fix in the dirty development worktree

**Files:**
- Modify only the matching Night Light/test/verification files under `D:\work\tools\hdr-sdr-brightness`

- [ ] **Step 1: Record dirty-worktree hashes and diff**

Capture the pre-sync hashes and relevant diffs for every overlapping file.

- [ ] **Step 2: Apply only the validated hotfix hunks**

Do not copy whole files. Add the public active-state behavior, cache invalidation
API/call, app-state tests, and verification/preflight wiring while preserving
all unrelated user changes.

- [ ] **Step 3: Run focused tests in the development worktree**

Run its app-state and wiring tests and compare the hotfix behavior with the
isolated branch.

### Task 10: Handoff and stop before publication

**Files:**
- Modify: `AI_HANDOFF.md`
- Modify: `D:\work\tools\handover.md`

- [ ] **Step 1: Update both handoffs**

Record cause, changed files, RED/GREEN evidence, automated verification, package
hashes, installation state, live-test results, restored Windows state, and
remaining risks.

- [ ] **Step 2: Report evidence and stop**

Provide the local candidate paths and validation results. Do not commit, push,
create a GitHub Release, upload a Store package, submit Partner Center, or send
Bilibili messages.
