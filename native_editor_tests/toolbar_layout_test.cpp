#include "../native_editor/editor_toolbar.h"

#include <windows.h>

#include <cstdio>
#include <cwchar>

namespace editor {
const wchar_t* FluentGlyphForEditorIcon(EditorIcon icon);
}

namespace {

bool TestToolbarItemGeometry() {
    editor::ToolbarLayout layout = editor::LayoutSelectionToolbar(
        RECT{100, 100, 1500, 700}, 1920, 1080);
    if (layout.bounds.left != 390 || layout.bounds.top != 710 ||
        layout.bounds.right - layout.bounds.left != 820 ||
        layout.items.size() != 14) {
        std::fprintf(stderr, "FAIL: selection toolbar geometry changed.\n");
        return false;
    }
    if (layout.items.front().action != editor::ToolbarAction::Cancel ||
        layout.items.front().rect.left != layout.bounds.left + 14 ||
        layout.items[6].action != editor::ToolbarAction::Undo ||
        layout.items[6].rect.left != layout.bounds.left + 351 ||
        layout.items.back().action != editor::ToolbarAction::Copy ||
        layout.items.back().rect.left != layout.bounds.left + 755) {
        std::fprintf(stderr, "FAIL: toolbar action order or separator spacing changed.\n");
        return false;
    }
    return true;
}

bool TestToolbarMovesAboveAndClamps() {
    editor::ToolbarLayout layout = editor::LayoutSelectionToolbar(
        RECT{0, 760, 400, 1060}, 1920, 1080);
    if (layout.bounds.left != 12 || layout.bounds.top != 688) {
        std::fprintf(stderr, "FAIL: toolbar did not clamp above a bottom-edge selection.\n");
        return false;
    }
    return true;
}

bool TestOptionPanelPlacement() {
    RECT anchor{500, 710, 546, 752};
    RECT toolbar{390, 710, 1210, 772};
    RECT options = editor::LayoutToolbarOptions(anchor, toolbar, 174, 48, 1920, 1080);
    if (options.left != 436 || options.top != 654 ||
        options.right - options.left != 174 || options.bottom - options.top != 48) {
        std::fprintf(stderr, "FAIL: toolbar option panel placement changed.\n");
        return false;
    }
    return true;
}

bool TestPreviousEditorFluentGlyphMapping() {
    struct ExpectedGlyph {
        editor::EditorIcon icon;
        const wchar_t* glyph;
    };
    const ExpectedGlyph expected[] = {
        {editor::EditorIcon::Ellipse, L"\xEA3A"},
        {editor::EditorIcon::Pen, L"\xED63"},
        {editor::EditorIcon::Color, L"\xE790"},
        {editor::EditorIcon::Undo, L"\xE7A7"},
        {editor::EditorIcon::Redo, L"\xE7A6"},
        {editor::EditorIcon::Reset, L"\xE72C"},
        {editor::EditorIcon::Save, L"\xE74E"}
    };
    for (const ExpectedGlyph& item : expected) {
        const wchar_t* actual = editor::FluentGlyphForEditorIcon(item.icon);
        if (!actual || std::wcscmp(actual, item.glyph) != 0) {
            std::fprintf(stderr, "FAIL: native toolbar no longer uses the previous Fluent glyph mapping.\n");
            return false;
        }
    }
    const editor::EditorIcon custom[] = {
        editor::EditorIcon::Cancel,
        editor::EditorIcon::Marker,
        editor::EditorIcon::Mosaic,
        editor::EditorIcon::Low,
        editor::EditorIcon::Balanced,
        editor::EditorIcon::High,
        editor::EditorIcon::Copy
    };
    for (editor::EditorIcon icon : custom) {
        const wchar_t* glyph = editor::FluentGlyphForEditorIcon(icon);
        if (glyph && glyph[0] != L'\0') {
            std::fprintf(stderr, "FAIL: a previously custom toolbar icon was replaced by a glyph.\n");
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!TestToolbarItemGeometry()) return 1;
    if (!TestToolbarMovesAboveAndClamps()) return 1;
    if (!TestOptionPanelPlacement()) return 1;
    if (!TestPreviousEditorFluentGlyphMapping()) return 1;
    std::printf("PASS: native toolbar layout preserves action order and screen bounds.\n");
    return 0;
}
