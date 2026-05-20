#pragma once

#include <initializer_list>
#include <string>
#include <vector>

enum LanguageChoice {
    LangAuto = 0,
    LangEnglish = 1,
    LangChinese = 2,
    LangKorean = 3,
    LangJapanese = 4,
    LangRussian = 5,
    LangChineseTraditional = 6,
    LangGerman = 7
};

enum TextId {
    TxtStarting,
    TxtDisplayName,
    TxtMenuApply,
    TxtMenuSettings,
    TxtMenuStartup,
    TxtMenuDisplaySettings,
    TxtMenuNightLightSettings,
    TxtMenuExit,
    TxtSettingsTitle,
    TxtSettingsSubtitle,
    TxtLanguage,
    TxtLanguageAuto,
    TxtLanguageEnglish,
    TxtLanguageChinese,
    TxtLanguageKorean,
    TxtLanguageJapanese,
    TxtLanguageRussian,
    TxtLanguageChineseTraditional,
    TxtLanguageGerman,
    TxtBrightnessGroup,
    TxtDay,
    TxtNight,
    TxtAppearanceGroup,
    TxtBehaviorGroup,
    TxtFollowNightLight,
    TxtAutoRestoreManual,
    TxtStartWithWindows,
    TxtFallbackSchedule,
    TxtSwitchMode,
    TxtFollowSystem,
    TxtUseDefaultSchedule,
    TxtNightLightUnavailable,
    TxtFollowSystemHint,
    TxtNightStarts,
    TxtDayStarts,
    TxtOk,
    TxtApply,
    TxtCancel,
    TxtInvalidDayBrightness,
    TxtInvalidNightBrightness,
    TxtInvalidSchedule,
    TxtSourceNightLight,
    TxtSourceFixed,
    TxtSourcePreview,
    TxtFixedScheduleSource,
    TxtAppliedDwm,
    TxtAppliedToDisplays,
    TxtNoHdrDisplay,
    TxtNoHdrShort,
    TxtNoHdrScreen,
    TxtApplyFailed,
    TxtVia,
    TxtNotifyTitle,
    TxtNotifyBody,
    TxtNotifyDialogTitle,
    TxtManualRestoreOff,
    TxtNotifyManualRestoreHint,
    TxtUnsavedChanges,
    TxtStatusBrightness,
    TxtTrayRestoringBrightness,
    TxtTrayBrightness,
    TxtHdrDisplays,
    TxtAutoRestoreShort,
    TxtOn,
    TxtOff,
    TxtPercentValue,
    TxtUnknownPercent,
    TxtHdrSyncAll,
    TxtHdrSyncPartial,
    TxtHdrPillStatus,
    TxtHeroStatus,
    TxtSdrContent,
    TxtHdrContent,
    TxtCurrentState,
    TxtRestoreOn,
    TxtRestoreOff,
    TxtMenuSupport,
    TxtSupportAuthor,
    TxtSupportAuthorSubtitle,
    TxtSupportCodeLabel,
    TxtSupportDonate,
    TxtSupportActivate,
    TxtSupporterBadge,
    TxtSupporterTooltip,
    TxtSupportThanks,
    TxtSupportInvalidCode,
    TxtSupportReminderTitle,
    TxtSupportReminderBody,
    TxtMenuHdrCalibration,
    TxtHdrCalibrationTitle,
    TxtHdrCalibrationLink,
    TxtHdrCalibrationHint,
    TxtHdrCalibrationBody,
    TxtTextCount
};

struct LanguageOption {
    int id;
};

int NormalizeLanguageChoice(int language);
const LanguageOption* LanguageOptions();
int LanguageOptionCount();
const wchar_t* LocalizedText(int language, TextId id);
const wchar_t* LocalizedLanguageName(int uiLanguage, int language);
bool IsLocalizedTextValue(TextId id, const std::wstring& value);
std::wstring FormatLocalizedText(int language, TextId id, const std::vector<std::wstring>& args);
std::wstring FormatLocalizedText(int language, TextId id, std::initializer_list<std::wstring> args);
