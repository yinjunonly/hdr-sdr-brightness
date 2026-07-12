#pragma once

#include "editor_toolbar.h"

#include <windows.h>

namespace editor {

HWND CreateToolbarTooltip(HWND owner);
void UpdateToolbarTooltip(HWND tooltip,
                          HWND owner,
                          const ToolbarLayout& layout,
                          int language);

}  // namespace editor
