#pragma once

#include <windows.h>

namespace editor {

class EditorSingleInstance {
public:
    EditorSingleInstance() = default;
    ~EditorSingleInstance();

    EditorSingleInstance(const EditorSingleInstance&) = delete;
    EditorSingleInstance& operator=(const EditorSingleInstance&) = delete;

    bool AcquireLatest();

private:
    void Release();

    HANDLE mutex_ = nullptr;
    HANDLE mapping_ = nullptr;
    volatile LONG* latestGeneration_ = nullptr;
    LONG generation_ = 0;
    bool ownsMutex_ = false;
};

}  // namespace editor
