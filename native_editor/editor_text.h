#pragma once

namespace editor {

enum class EditorTextId {
    PreviewTitle,
    RegionTitle,
    SelectHint,
    SaveDialogTitle,
    CopiedStatus,
    SavedStatus,
    ToolbarCancel,
    ToolbarMarker,
    ToolbarEllipse,
    ToolbarPen,
    ToolbarMosaic,
    ToolbarColor,
    ToolbarUndo,
    ToolbarRedo,
    ToolbarReset,
    ToolbarLow,
    ToolbarBalanced,
    ToolbarHigh,
    ToolbarSave,
    ToolbarCopy,
    Count
};

const wchar_t* GetEditorText(int language, EditorTextId id);
const wchar_t* GetEditorFontName(int language);

}  // namespace editor
