# v1.1.5 Windows Night Light Follow Hotfix Design

## Goal

Release a narrowly scoped v1.1.5 candidate in which “Follow Windows” tracks the
current Windows Night Light state while the app remains running, regardless of
whether Windows changed that state through sunset/sunrise, custom hours, or the
manual enable/disable control.

## Approved constraints

- Start from clean tag `v1.1.4` in an isolated hotfix branch.
- Do not import unrelated changes from the dirty development worktree.
- Normal changes should be applied immediately from the CloudStore registry
  notification; a 15-second timer is the fallback.
- If the current Night Light state cannot be read, use the app's built-in
  schedule and show the existing unavailable/fallback state in settings.
- Validate both the desktop build and a locally installed developer-signed Store
  MSIX.
- Change Windows Night Light only through the Windows Settings UI during live
  testing, record the original configuration, and restore it afterwards.
- Stop after local installation and verification. Do not commit, publish, submit
  to Partner Center, or reply on Bilibili without the user's later instruction.

## Considered approaches

### A. Restore only the v1.1.0 cache invalidation

This is the smallest textual patch, but it still rejects Windows custom-hour
schedules and can therefore reproduce the reporter's second screenshot. It does
not satisfy the public “Follow Windows Night Light” wording.

### B. Follow the actual active state and restore cache invalidation

This is the selected approach. `night_mode::Decide` uses the current readable
Night Light active state whenever Follow Windows is selected. The Windows
schedule type is irrelevant. A dedicated active-state cache invalidation keeps
schedule-independent polling inexpensive and lets the 15-second timer recover
from missed registry notifications.

### C. Parse and duplicate every Windows scheduling mode

This would mirror sunset and custom-hour schedule data inside the app. It is
more fragile, duplicates Windows policy, and still risks disagreeing with
manual overrides. It is unnecessary when Windows already exposes the current
active state.

## Runtime design

`night_mode::Decide` owns one decision:

1. When Follow Windows is enabled, read the cached current Night Light state.
2. If that state is known, return it with
   `DecisionSourceWindowsNightLight`.
3. If it is unknown, calculate day/night from the app's built-in fixed schedule.

`night_mode::CanFollowWindowsNightLight` reports whether the current active
state is readable, not whether Windows selected the sunset schedule.

The CloudStore registry watcher continues to invalidate all Night Light caches
and request an immediate brightness application. Independently, every
`kRecheckTimer` tick invalidates only the active-state cache before applying
brightness. This bounds a missed-notification delay to 15 seconds.

## Test design

The native app-state test uses the real `night_mode.cpp` public API with test
implementations of registry/process dependencies. It must cover:

- active state off selects day;
- active state on selects night when Windows uses sunset/sunrise;
- active state on selects night when Windows uses custom hours;
- active state on selects night when Windows scheduling is disabled and the
  user enabled Night Light manually;
- an unreadable state falls back to the app schedule;
- changing active state remains cached until
  `InvalidateActiveStateCache`, then becomes visible.

The source contract test must require
`InvalidateActiveStateCache()` before `ApplyCurrentBrightness(false)` in the
15-second timer branch.

`verify.ps1` must run `app_tests/build.ps1`. A release preflight must verify that
the app-state suite remains wired, verify that both package scripts invoke the
preflight, reject dirty tracked source for formal packaging, and run the
non-clipboard full verification before producing artifacts. Local prerelease
packages may opt into an explicit dirty-source allowance.

## Live validation

Use distinct configured day/night levels already present on the machine and
observe the real SDR brightness:

1. Desktop build: manually enable Night Light and confirm night brightness
   within 15 seconds; disable and confirm day brightness within 15 seconds.
2. Store MSIX: repeat both directions.
3. Store MSIX: set a custom Windows start time through Settings, wait for the
   scheduled transition, and confirm the night level within 15 seconds.
4. Restore the original Night Light schedule, active state, and application
   settings.

Any failed automated or live check blocks release.
