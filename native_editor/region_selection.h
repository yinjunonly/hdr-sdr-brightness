#pragma once

#include "editor_options.h"
#include "image_document.h"

#include <windows.h>

namespace editor {

int ShowRegionSelectionWindow(HINSTANCE instance,
                              const EditorOptions& options,
                              BgraImage image);

}  // namespace editor
