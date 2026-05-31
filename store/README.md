# Microsoft Store Packaging

This folder contains the MSIX manifest template and Store certification notes for the Microsoft Store submission.

Partner Center identity values in `AppxManifest.xml.in`:

- `Package/Identity/Name`: `InjunaId.HDRSDRBrightnessAssistant`
- `Package/Identity/Publisher`: `CN=81C6B903-8D8C-435A-B42E-3678C731A484`
- `Package/Properties/PublisherDisplayName`: `InjunaId`

Submission content drafts:

- `listing-draft.md`: Store listing text, features, keywords, screenshots, and pricing notes.
- `privacy-policy-draft.md`: privacy policy text for the required privacy policy URL.
- `support-page-draft.md`: support page text for the required support URL.
- `partner-center-fields.md`: Partner Center property and submission checklist.
- `pricing-strategy.md`: paid pricing, global market, and launch discount decisions.
- `screenshot-plan.md`: recommended screenshots and capture checklist.

Create an unsigned MSIX plus a Partner Center upload file only when intentionally preparing a release:

```powershell
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.0.10 -Clean
```

Expected outputs:

- `dist\HdrSdrBrightness-1.0.10-win64.msix`
- `dist\HdrSdrBrightness-1.0.10-win64.msixupload`

Upload the `.msixupload` file to Partner Center. Microsoft recommends `.msixupload` for Windows 10/11 Store submissions, and the upload file can contain the `.msix` plus an optional `.appxsym` symbol package for crash analytics.

Useful packaging options:

```powershell
# Reuse the current Store build and only package it.
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.0.10 -SkipBuild

# Create only the MSIX, without the Partner Center upload wrapper.
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.0.10 -SkipUpload

# Include an optional symbol package if one is generated later.
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.0.10 -AppxSymPath .\dist\HdrSdrBrightness-1.0.10-win64.appxsym
```

The older `package.ps1` script creates a ZIP for direct distribution. Do not upload that ZIP as a Microsoft Store package.

For local sideload testing, sign the generated MSIX with a trusted test certificate before installing it. Partner Center will validate the uploaded Store package against the reserved identity.

Before upload:

1. Build with `.\build.ps1 -Store` and smoke test the app.
2. Run the Windows App Certification Kit against the signed package.
3. Confirm the Store build hides donation/supporter-code UI.
4. Confirm startup is disabled until the user enables it in the app or Windows Settings.
5. Upload only after the final package is intentionally prepared for release.
