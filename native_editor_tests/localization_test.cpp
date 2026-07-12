#include "../native_editor/editor_text.h"

#include <cstdio>

int main() {
    for (int language = 1; language <= 7; ++language) {
        if (!editor::GetEditorFontName(language) || !*editor::GetEditorFontName(language)) return 1;
        for (int value = 0; value < static_cast<int>(editor::EditorTextId::Count); ++value) {
            const wchar_t* text = editor::GetEditorText(
                language, static_cast<editor::EditorTextId>(value));
            if (!text || !*text) {
                std::fprintf(stderr, "FAIL: language %d is missing editor string %d.\n", language, value);
                return 1;
            }
        }
    }
    std::printf("PASS: all seven native editor languages are complete.\n");
    return 0;
}
