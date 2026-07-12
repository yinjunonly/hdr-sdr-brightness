#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

#include "registry_util.h"

bool ReadDwordValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, DWORD* value) {
    HKEY key = NULL;
    LONG rc = RegOpenKeyExW(root, keyPath, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD data = 0;
    rc = RegQueryValueExW(key, valueName, NULL, &type, reinterpret_cast<LPBYTE>(&data), &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(DWORD)) return false;
    *value = data;
    return true;
}

void WriteDwordValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, DWORD value) {
    HKEY key = NULL;
    LONG rc = RegCreateKeyExW(root, keyPath, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return;
    RegSetValueExW(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

bool ReadStringValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, std::wstring* value) {
    HKEY key = NULL;
    LONG rc = RegOpenKeyExW(root, keyPath, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    rc = RegQueryValueExW(key, valueName, NULL, &type, NULL, &size);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || size < sizeof(wchar_t)) {
        RegCloseKey(key);
        return false;
    }

    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
    rc = RegQueryValueExW(key, valueName, NULL, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;

    buffer.back() = L'\0';
    if (value) *value = buffer.data();
    return true;
}

void WriteStringValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, const std::wstring& value) {
    HKEY key = NULL;
    LONG rc = RegCreateKeyExW(root, keyPath, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return;
    RegSetValueExW(key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

bool ReadBinaryValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, std::vector<BYTE>* data) {
    HKEY key = NULL;
    LONG rc = RegOpenKeyExW(root, keyPath, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    rc = RegQueryValueExW(key, valueName, NULL, &type, NULL, &size);
    if (rc != ERROR_SUCCESS || type != REG_BINARY || size == 0) {
        RegCloseKey(key);
        return false;
    }

    std::vector<BYTE> buffer(size);
    rc = RegQueryValueExW(key, valueName, NULL, &type, buffer.data(), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_BINARY) return false;

    buffer.resize(size);
    data->swap(buffer);
    return true;
}
