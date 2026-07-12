#pragma once

#include <windows.h>
#include <string>
#include <vector>

bool ReadDwordValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, DWORD* value);
void WriteDwordValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, DWORD value);
bool ReadStringValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, std::wstring* value);
void WriteStringValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, const std::wstring& value);
bool ReadBinaryValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, std::vector<BYTE>* data);
