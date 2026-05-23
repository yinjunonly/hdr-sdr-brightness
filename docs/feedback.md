# User Feedback

This document collects early public feedback about HDR SDR Brightness. It is not telemetry and should not be treated as a scientific survey. The goal is to preserve useful patterns from user comments so future defaults, documentation, and feature ideas can be grounded in real usage.

## 2026-05 Xiaoheihe Post

Source:

```text
https://www.xiaoheihe.cn/app/bbs/link/181874424?action=save
```

Context:

- The post introduced HDR SDR Brightness as a lightweight Windows tray utility for managing SDR content brightness while Windows HDR is enabled.
- The discussion was mostly from Windows HDR / OLED / MiniLED / HDR monitor users.
- Users compared the app with the built-in Windows SDR content brightness slider.

## Brightness Ranges Mentioned

Early comments mentioned these SDR content brightness habits:

| Reported range | Context |
| --- | --- |
| `0` | Some users keep SDR content brightness at the lowest setting when HDR is always on, especially for comfortable browsing and OLED burn-in caution. |
| `30-40` | One user reported this range as visually comfortable. |
| `40` | Another user reported using around 40 as a common setting. |

These comments support keeping the default values conservative:

| Setting | Current default |
| --- | ---: |
| Day SDR content brightness | `40%` |
| Night SDR content brightness | `25%` |

The comments also suggest that the best range depends heavily on panel type, room lighting, and whether the user keeps HDR enabled all day.

## Confirmed Product Positioning

Several users pointed out that Windows 11 already includes an SDR content brightness slider. This is useful feedback for wording and onboarding.

The app should be described as an automation helper around the Windows setting, not as a replacement for it:

- Windows already provides the SDR content brightness slider.
- The app makes it easier to use separate day/night values.
- The app can follow Windows Night Light or a fallback schedule.
- The app can restore configured brightness after manual changes.
- The app stays in the tray so users do not need to open Windows Settings repeatedly.

Suggested short positioning:

```text
HDR SDR Brightness automates Windows SDR content brightness while HDR is enabled, so you do not have to adjust the system slider manually throughout the day.
```

## Feature Ideas From Feedback

### HDR Screenshot / Recording Overexposure

One user said HDR screenshots being overexposed is the most uncomfortable issue.

This is separate from SDR content brightness control and may involve Windows HDR capture, tone mapping, GPU drivers, screenshot tools, or app-specific capture paths.

Potential next steps:

- Investigate how Windows captures HDR desktop content in common screenshot paths.
- Test Snipping Tool, Xbox Game Bar, Print Screen, OBS, and browser screenshots with HDR enabled.
- Document whether this is something the app can detect, guide, or help route to better tools.
- Avoid promising a fix until the capture pipeline is better understood.

## Documentation Follow-Ups

Possible README or FAQ additions:

- Explain the difference between Windows' built-in SDR content brightness slider and this app's automation.
- Add a small table of early user-reported ranges: `0`, `30-40`, and `40`.
- Mention that OLED users may prefer lower SDR brightness for browsing and burn-in caution.
- Mention that HDR screenshot overexposure is a known adjacent issue under investigation, not currently a core feature.

## Product Notes

- The day/night model matches real user behavior: room lighting and time of day affect comfort.
- The current `40%` day default appears reasonable based on early comments.
- The current `25%` night default remains conservative and may be suitable for dim rooms or OLED users, though more feedback is needed.
- Community wording should emphasize comfort, automation, and low resource use rather than claiming to be an HDR calibration tool.