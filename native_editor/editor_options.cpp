#include "editor_options.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdlib>

namespace editor {

namespace {

std::wstring ReplaceExtension(const std::wstring& path, const wchar_t* extension) {
    size_t separator = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos ||
        (separator != std::wstring::npos && dot < separator)) {
        return path + extension;
    }
    return path.substr(0, dot) + extension;
}

bool Fail(const wchar_t* message, std::wstring* error) {
    if (error) *error = message;
    return false;
}

}  // namespace

bool ParseEditorOptions(EditorOptions* options, std::wstring* error) {
    if (!options) return Fail(L"No editor options destination was provided.", error);
    *options = EditorOptions{};
    if (error) error->clear();

    int count = 0;
    LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args) return Fail(L"Could not parse the editor command line.", error);
    for (int index = 1; index < count; ++index) {
        std::wstring name = args[index];
        if (name == L"--warmup") {
            options->warmup = true;
        } else if ((name == L"--select-file" || name == L"--edit-file") && index + 1 < count) {
            options->selectMode = name == L"--select-file";
            options->imagePath = args[++index];
        } else if (name == L"--output" && index + 1 < count) {
            options->outputPath = args[++index];
        } else if (name == L"--lang" && index + 1 < count) {
            options->language = _wtoi(args[++index]);
        } else if (name == L"--skip-initial-copy") {
            options->skipInitialCopy = true;
        } else if (name == L"--allow-window-capture") {
            options->allowWindowCaptureForTests = true;
        }
    }
    LocalFree(args);

    if (!options->warmup && options->imagePath.empty()) {
        return Fail(L"Missing --select-file or --edit-file image path.", error);
    }
    if (!options->warmup && options->outputPath.empty()) {
        options->outputPath = ReplaceExtension(options->imagePath, L".png");
    }
    return true;
}

}  // namespace editor
