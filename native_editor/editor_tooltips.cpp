#include "editor_tooltips.h"

#include "editor_text.h"

#include <commctrl.h>

namespace editor {

namespace {

EditorTextId TextIdFor(ToolbarAction action) {
    switch (action) {
    case ToolbarAction::Cancel: return EditorTextId::ToolbarCancel;
    case ToolbarAction::ToolMarker: return EditorTextId::ToolbarMarker;
    case ToolbarAction::ToolEllipse: return EditorTextId::ToolbarEllipse;
    case ToolbarAction::ToolPen: return EditorTextId::ToolbarPen;
    case ToolbarAction::ToolMosaic: return EditorTextId::ToolbarMosaic;
    case ToolbarAction::Color: return EditorTextId::ToolbarColor;
    case ToolbarAction::Undo: return EditorTextId::ToolbarUndo;
    case ToolbarAction::Redo: return EditorTextId::ToolbarRedo;
    case ToolbarAction::Reset: return EditorTextId::ToolbarReset;
    case ToolbarAction::PresetLow: return EditorTextId::ToolbarLow;
    case ToolbarAction::PresetBalanced: return EditorTextId::ToolbarBalanced;
    case ToolbarAction::PresetHigh: return EditorTextId::ToolbarHigh;
    case ToolbarAction::Save: return EditorTextId::ToolbarSave;
    case ToolbarAction::Copy: return EditorTextId::ToolbarCopy;
    }
    return EditorTextId::ToolbarCancel;
}

}  // namespace

HWND CreateToolbarTooltip(HWND owner) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);
    HWND tooltip = CreateWindowExW(WS_EX_TOPMOST,
                                   TOOLTIPS_CLASSW,
                                   nullptr,
                                   WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (tooltip) {
        SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 360);
        SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 500);
    }
    return tooltip;
}

void UpdateToolbarTooltip(HWND tooltip,
                          HWND owner,
                          const ToolbarLayout& layout,
                          int language) {
    if (!tooltip || !owner) return;
    for (UINT_PTR id = 1; id <= 14; ++id) {
        TOOLINFOW remove{};
        remove.cbSize = sizeof(remove);
        remove.hwnd = owner;
        remove.uId = id;
        SendMessageW(tooltip, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&remove));
    }
    for (size_t index = 0; index < layout.items.size(); ++index) {
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS;
        tool.hwnd = owner;
        tool.uId = index + 1;
        tool.rect = layout.items[index].rect;
        tool.lpszText = const_cast<wchar_t*>(GetEditorText(
            language, TextIdFor(layout.items[index].action)));
        SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
    }
}

}  // namespace editor
