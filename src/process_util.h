#pragma once

#include <windows.h>
#include <string>

std::wstring QuotePath(const std::wstring& path);
std::wstring QuoteCommandLineArgument(const std::wstring& value);
std::wstring QuotePowerShellString(const std::wstring& value);
std::wstring GetExePath();
std::wstring DirectoryFromPath(const std::wstring& path);
std::wstring JoinPath(const std::wstring& directory, const std::wstring& relative);
std::wstring GetCloudSettingsReaderPath();
bool FileExists(const std::wstring& path);

bool LaunchDetached(const std::wstring& commandLine, const std::wstring& workingDirectory);
bool LaunchDetachedHidden(const std::wstring& commandLine, const std::wstring& workingDirectory);
bool RunProcessCapture(const std::wstring& commandLine, DWORD timeoutMs, std::string* output);
bool RunHiddenCommand(const std::wstring& commandLine, DWORD timeoutMs);
bool RunHiddenCommandExitCode(const std::wstring& commandLine, DWORD timeoutMs, DWORD* exitCode);
