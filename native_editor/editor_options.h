#pragma once

#include <string>

namespace editor {

struct EditorOptions {
    bool warmup = false;
    bool selectMode = false;
    std::wstring imagePath;
    std::wstring outputPath;
    int language = 2;
    bool skipInitialCopy = false;
    bool allowWindowCaptureForTests = false;
};

bool ParseEditorOptions(EditorOptions* options, std::wstring* error);

}  // namespace editor
