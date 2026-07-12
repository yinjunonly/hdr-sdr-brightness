#pragma once

#include <string>

namespace capture_paths {

std::wstring FullscreenNotificationBmpPath();
std::wstring RegionEditBmpPath();
std::wstring ReplaceExtension(const std::wstring& path, const std::wstring& extension);

}  // namespace capture_paths
