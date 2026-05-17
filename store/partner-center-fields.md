# Partner Center Fields

Use this as the submission checklist when filling Partner Center.

## Product Setup

```text
Reserved product name: HDR SDR Brightness Assistant
Package identity name: InjunaId.HDRSDRBrightnessAssistant
Publisher: CN=81C6B903-8D8C-435A-B42E-3678C731A484
Publisher display name: InjunaId
Category: Utilities & tools
Pricing: Paid
Base price: low-volume tier, target the tier closest to US$1.99
Launch sale: 50% off for 14 days, offered to everyone, target the tier closest to US$0.99
Free trial: Time-limited, 7 days, full functionality
Markets: all possible markets, including China, and include future markets when available
Privacy policy URL: https://github.com/yinjunonly/hdr-sdr-brightness/blob/main/docs/privacy-policy.md
Support URL: https://github.com/yinjunonly/hdr-sdr-brightness/blob/main/docs/support.md
Website URL: optional
```

## App Properties

Suggested answers:

```text
Does this app access the internet? No for core app functionality.
Does this app require sign-in? No.
Does this app include in-app purchases? No.
Does this app include subscriptions? No.
Does this app offer a free trial? Yes, Store-managed time-limited trial, 7 days.
Does this app include ads? No.
Does this app collect personal information? No.
Does this app use location? No.
Does this app use microphone/camera/contacts/calendar? No.
Does this app install drivers or services? No.
Does this app run at startup? Optional, only after user enables Start with Windows.
Target device family: Windows.Desktop
Minimum OS: Windows 10 version 1809 / 10.0.17763.0
Architecture: x64
```

Age rating guidance:

```text
Non-game utility.
No violence.
No sexual content.
No gambling.
No user-generated content.
No unrestricted web access.
No social interaction.
No location sharing.
No personal data collection by the app.
```

## Pricing

Suggested Store pricing strategy:

```text
One-time paid app purchase.
Base price: choose the low paid tier closest to US$1.99.
Launch sale: 50% off for the first 14 days, offered to everyone.
Launch sale target: choose the sale tier closest to US$0.99.
China: keep China included and use the Store-recommended local price for the selected tier unless Partner Center shows a clearly unreasonable conversion.
Markets: all possible markets, including China; keep future markets included when Partner Center offers that option.
Free trial: time-limited, 7 days, full functionality.
No in-app purchases.
No subscriptions.
No donation prompts.
No supporter-code UI in the Store build.
```

Submission choices:

```text
Price tier: low-volume, target US$1.99 equivalent.
Launch markets: global / all possible markets, including China.
Introductory sale: yes, 50% off for 14 days, offered to everyone.
Free trial: yes, time-limited 7-day trial with full functionality.
```

Rationale:

```text
The app is a focused utility, so a low one-time price reduces friction and fits the "cheap, higher-volume" strategy.
Avoid a free launch sale because it weakens the paid-app positioning and may attract low-intent installs.
A 7-day full-feature trial lets customers verify that SDR brightness control works on their HDR display before paying.
Do not use an unlimited or feature-limited trial unless app-side Store license checks are added later.
```

Age rating:

```text
Complete the Partner Center age rating questionnaire as a non-game utility.
Expected result: lowest/general audience rating, because the app has no violence, sexual content, gambling, user-generated content, unrestricted web access, social interaction, location sharing, or personal data collection by the app.
```

## Package Upload

Use only after explicitly preparing a Store package:

```powershell
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.0.6 -Clean
```

Upload:

```text
dist\HdrSdrBrightness-1.0.6-win64.msixupload
```

Do not upload:

```text
GitHub ZIP release package
Unsigned loose EXE
Desktop ZIP package from package.ps1
```

## Certification Notes

Paste or attach the content from:

```text
store\certification-notes.md
```
