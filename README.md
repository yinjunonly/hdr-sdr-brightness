# HDR SDR Brightness

[中文说明](README.zh-CN.md)

A minimal Windows tray utility that automatically adjusts **Windows SDR content brightness** while HDR is enabled.

It is useful for OLED, QD-OLED, MiniLED, HDR monitors, HDR TVs, and HDR-capable laptop displays where SDR apps become too bright, too dim, or washed out after enabling Windows HDR.

## Default Behavior

- Night SDR content brightness: `10`
- Day SDR content brightness: `25`
- Switching mode: follow Windows Night Light when available, otherwise use the default schedule
- Default schedule: night starts at `18:00`, day starts at `08:00`
- Applies the correct SDR content brightness immediately on startup
- Checks manual SDR brightness changes every 15 seconds and restores the configured value
- Shows a system notification when restoring a manual change
- Applies brightness changes gradually in both directions
- Manual-change restore can be disabled in Settings
- Clicking the restore notification opens a detail dialog

## Build

Requires MinGW `g++` and `windres`.

The project version is stored in `VERSION`. The current version is `1.0.0`.

```powershell
.\build.ps1
```

Output:

```text
bin\HdrSdrBrightness.exe
```

Build artifacts are generated under `bin\` and `obj\` and are intentionally ignored by Git.

To create a GitHub Release archive:

```powershell
.\package.ps1
```

Output:

```text
dist\HdrSdrBrightness-1.0.0-win64.zip
```

The archive contains only the executable, license, and readme files.

## Use

Run `bin\HdrSdrBrightness.exe`; the settings window opens and the app also stays in the system tray.

Use `bin\HdrSdrBrightness.exe --background` for a quiet tray-only launch. Start with Windows uses this mode.

Tray menu:

- Apply now
- Settings: language, SDR content brightness, fallback schedule, startup
- Start with Windows
- Open Display settings
- Exit

## UI And Icon

- English and Chinese UI with automatic language detection
- Light and dark modes follow the Windows app theme
- Custom-drawn Windows Settings-style surface with cards, switches, sliders, segmented controls, and hover states
- GDI+ antialiasing for rounded controls
- Tray hover shows a compact live summary: mode, target SDR content brightness, HDR display status, and auto-restore state
- Tray icon is optimized for small sizes with a high-contrast abstract mark

## Notes

Configuration is stored under the current user:

```text
HKCU\Software\OledHdrSdrSync
```

The old configuration path is kept for compatibility.

Start with Windows uses:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run\HdrSdrBrightness
```

Old `HdrSdrSync` and `OledHdrSdrSync` startup entries are migrated when settings are saved.

No administrator privileges are required.

## License

MIT License. See [LICENSE](LICENSE).

## Discovery Keywords

People often describe this problem in different ways. These keywords are included to make the project easier to find:

```text
Windows HDR SDR brightness
Windows HDR SDR content brightness
Windows SDR content brightness
HDR SDR brightness balance
HDR/SDR brightness balance
SDR content brightness slider
SDR content brightness too bright
SDR content too bright in HDR
SDR content too dark in HDR
Windows HDR too bright
Windows HDR too dim
Windows HDR washed out
Windows 11 HDR washed out
Windows 10 HDR washed out
Auto HDR washed out
HDR desktop too bright
HDR desktop too dim
HDR desktop washed out
HDR colors washed out
HDR looks washed out
OLED HDR brightness
QD-OLED HDR brightness
MiniLED HDR brightness
HDR monitor SDR brightness
HDR TV Windows SDR brightness
Windows Night Light SDR brightness
SDR white level
```
