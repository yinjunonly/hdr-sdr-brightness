#define WIN32_LEAN_AND_MEAN

#include "supporter_code.h"

#include <windows.h>

namespace supporter_code {
namespace {

UINT32 HashToken(const std::wstring& token) {
    const char salt[] = "HdrSdrBrightnessSupporterV1";
    UINT32 hash = 2166136261u;
    for (size_t i = 0; i < sizeof(salt) - 1; ++i) {
        hash ^= static_cast<BYTE>(salt[i]);
        hash *= 16777619u;
    }
    for (size_t i = 0; i < token.size(); ++i) {
        hash ^= static_cast<BYTE>(token[i] & 0xff);
        hash *= 16777619u;
    }
    return hash;
}

int HexValue(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
    if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
    return -1;
}

}  // namespace

bool IsAllowedCharacter(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') ||
           (ch >= L'A' && ch <= L'Z') ||
           (ch >= L'a' && ch <= L'z') ||
           ch == L'-';
}

std::wstring Normalize(const std::wstring& value) {
    std::wstring normalized;
    for (size_t i = 0; i < value.size(); ++i) {
        wchar_t ch = value[i];
        if (!IsAllowedCharacter(ch)) continue;
        if (ch >= L'a' && ch <= L'z') ch = static_cast<wchar_t>(ch - L'a' + L'A');
        normalized.push_back(ch);
    }
    return normalized;
}

bool IsValid(const std::wstring& value) {
    std::wstring code = Normalize(value);
    const std::wstring prefix = L"HDRSDR-";
    if (code.size() != prefix.size() + 8 + 1 + 4) return false;
    if (code.compare(0, prefix.size(), prefix) != 0) return false;
    if (code[prefix.size() + 8] != L'-') return false;

    std::wstring token = code.substr(prefix.size(), 8);
    for (size_t i = 0; i < token.size(); ++i) {
        wchar_t ch = token[i];
        if (!((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z'))) return false;
    }

    int checksum = 0;
    for (size_t i = prefix.size() + 9; i < code.size(); ++i) {
        int valuePart = HexValue(code[i]);
        if (valuePart < 0) return false;
        checksum = (checksum << 4) | valuePart;
    }
    return checksum == static_cast<int>(HashToken(token) & 0xffffu);
}

}  // namespace supporter_code
