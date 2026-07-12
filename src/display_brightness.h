#pragma once

#include <windows.h>

struct ApplyResult {
    bool ok;
    int targetCount;
    int successCount;
    LONG lastError;
    bool usedDwmFallback;
    bool changed;
    bool complete;
    UINT32 appliedLevel;

    ApplyResult()
        : ok(false),
          targetCount(0),
          successCount(0),
          lastError(ERROR_SUCCESS),
          usedDwmFallback(false),
          changed(false),
          complete(false),
          appliedLevel(0) {}
};

UINT32 BrightnessPercentToSdrLevel(int brightness);
ApplyResult ApplySdrLevelStep(UINT32 targetLevel, bool smooth, UINT32 fallbackCurrentLevel,
                              UINT32 transitionStepLevel);
ApplyResult CheckSdrBrightness(int brightness);
