#include "editor_window.h"

#include "preview_editor.h"
#include "region_selection.h"

#include <utility>

namespace editor {

int ShowEditorWindow(HINSTANCE instance, const EditorOptions& options, BgraImage image) {
    return options.selectMode
        ? ShowRegionSelectionWindow(instance, options, std::move(image))
        : ShowPreviewEditorWindow(instance, options, std::move(image));
}

}  // namespace editor
