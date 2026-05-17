# Certification Notes

HDR SDR Brightness Assistant is a Win32 desktop utility packaged as MSIX for Microsoft Store distribution.

The app runs as a tray utility and changes the Windows HDR SDR content brightness level for displays where Windows HDR is enabled. It does not install drivers, services, shell extensions, browser components, or secondary software.

The app stores settings locally under the current user's registry profile. It does not require an account and does not collect or transmit telemetry, personal data, display data, or supporter codes.

The Store build hides the external support/donation entry points and supporter-code UI. The app is sold as a paid Microsoft Store app with a Store-managed time-limited free trial. During the trial and after purchase, the installed app is fully usable without in-app purchases, donation prompts, additional payment, account sign-in, subscriptions, or unlock codes.

The Store build does not create Run-key entries or scheduled tasks for startup. Startup integration uses the packaged desktop `windows.startupTask` manifest extension, launches the app with `--background`, and is enabled only when the user turns on Start with Windows in the app.

The startup task is declared with `Enabled="false"` in the package manifest. The app requests startup permission through the Windows packaged-app startup API and respects the user's Startup settings state.

Suggested test path:

1. Install the MSIX package.
2. Launch "HDR SDR Brightness Assistant" from Start.
3. Open settings from the tray icon.
4. Adjust Day and Night SDR brightness values.
5. Enable Windows HDR on at least one display and verify "Apply now" updates SDR content brightness.
6. Verify the app exits cleanly from the tray menu.
