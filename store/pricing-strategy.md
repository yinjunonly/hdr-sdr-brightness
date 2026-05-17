# Store Pricing Strategy

Decision:

```text
Paid Microsoft Store app.
Cheap, higher-volume positioning.
Global availability, including China.
Introductory sale enabled.
Time-limited free trial enabled.
```

Recommended Partner Center settings:

```text
Base price: choose the low paid tier closest to US$1.99.
Sale pricing: 50% off for the first 14 days.
Sale audience: Everyone.
Sale target: tier closest to US$0.99.
Markets: all possible markets, including China.
Future markets: include when Partner Center offers the option.
Free trial: Time-limited, 7 days, full functionality.
In-app purchases: No.
Subscriptions: No.
```

China/local pricing:

```text
Use Microsoft Store's recommended local price for the selected base tier and sale tier.
Do not manually raise the China price for the first release.
Only customize regional prices later if Partner Center's recommended conversion is clearly out of line with local expectations.
```

Launch sale:

```text
Use a Store sale instead of publishing at a temporary lower base price.
A sale shows customers the discounted price as a limited-time promotion.
Do not discount to Free for launch.
```

Free trial:

```text
Enable a Store-managed time-limited free trial.
Recommended duration: 7 days.
Trial behavior: full functionality during the trial.
No feature-limited trial UI, no in-app purchase prompts, no supporter-code UI, and no app-side trial nags.
After the trial, purchase/licensing is handled by Microsoft Store.
```

Reasoning:

```text
HDR SDR Brightness Assistant is a focused daily-use utility. The best first-release pricing posture is low-friction paid acquisition rather than a high utility price.

US$1.99 equivalent keeps the regular price inexpensive while still signaling that the Store build is the paid, clean version with no donation prompts or unlock codes.

A 50% launch sale creates a visible introductory offer without making the app look permanently free or ad-supported.

A 7-day full-feature trial lets users verify that SDR brightness control works on their HDR display before paying. This is important because HDR behavior depends on Windows version, display hardware, GPU driver, and monitor configuration.
```
