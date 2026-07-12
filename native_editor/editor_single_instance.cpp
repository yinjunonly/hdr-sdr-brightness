#include "editor_single_instance.h"

#include <windows.h>

#include <cwchar>

namespace editor {

namespace {

const wchar_t kEditorMutexName[] = L"Local\\HdrSdrNativeEditorSingleInstance";
const wchar_t kEditorGenerationName[] = L"Local\\HdrSdrNativeEditorGeneration";
const wchar_t kRegionWindowClass[] = L"HdrSdrNativeEditorRegionWindow";
const wchar_t kPreviewWindowClass[] = L"HdrSdrNativeEditorPreviewWindow";

BOOL CALLBACK CloseEditorWindow(HWND hwnd, LPARAM) {
    wchar_t className[128] = {};
    if (GetClassNameW(hwnd, className,
                      static_cast<int>(sizeof(className) / sizeof(className[0]))) == 0) {
        return TRUE;
    }
    if (std::wcscmp(className, kRegionWindowClass) == 0 ||
        std::wcscmp(className, kPreviewWindowClass) == 0) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

void CloseExistingEditorWindows() {
    EnumWindows(CloseEditorWindow, 0);
}

bool Acquired(DWORD waitResult) {
    return waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED;
}

}  // namespace

EditorSingleInstance::~EditorSingleInstance() {
    Release();
}

void EditorSingleInstance::Release() {
    if (ownsMutex_ && mutex_) ReleaseMutex(mutex_);
    ownsMutex_ = false;
    if (latestGeneration_) UnmapViewOfFile(const_cast<LONG*>(latestGeneration_));
    latestGeneration_ = nullptr;
    if (mapping_) CloseHandle(mapping_);
    mapping_ = nullptr;
    if (mutex_) CloseHandle(mutex_);
    mutex_ = nullptr;
}

bool EditorSingleInstance::AcquireLatest() {
    Release();
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, sizeof(LONG), kEditorGenerationName);
    if (!mapping_) return true;
    latestGeneration_ = static_cast<volatile LONG*>(
        MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG)));
    if (!latestGeneration_) {
        Release();
        return true;
    }
    generation_ = InterlockedIncrement(latestGeneration_);

    mutex_ = CreateMutexW(nullptr, FALSE, kEditorMutexName);
    if (!mutex_) {
        Release();
        return true;
    }

    DWORD wait = WaitForSingleObject(mutex_, 0);
    if (Acquired(wait)) {
        ownsMutex_ = true;
        if (*latestGeneration_ == generation_) return true;
        Release();
        return false;
    }
    if (wait != WAIT_TIMEOUT) {
        Release();
        return true;
    }

    CloseExistingEditorWindows();
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 5000) {
        if (*latestGeneration_ != generation_) {
            Release();
            return false;
        }
        wait = WaitForSingleObject(mutex_, 25);
        if (Acquired(wait)) {
            ownsMutex_ = true;
            if (*latestGeneration_ == generation_) return true;
            Release();
            return false;
        }
        if (wait != WAIT_TIMEOUT) {
            Release();
            return true;
        }
        CloseExistingEditorWindows();
    }

    Release();
    return false;
}

}  // namespace editor
