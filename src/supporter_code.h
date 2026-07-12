#pragma once

#include <string>

namespace supporter_code {

bool IsAllowedCharacter(wchar_t ch);
std::wstring Normalize(const std::wstring& value);
bool IsValid(const std::wstring& value);

}  // namespace supporter_code
