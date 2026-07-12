#pragma once

#include <windows.h>

#include <string>

namespace tray_icon {

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    void Add(HINSTANCE instance, HWND window, UINT callbackMessage, int iconResourceId,
             const std::wstring& tooltip);
    void UpdateTip(const std::wstring& tooltip);
    void ShowNotification(const std::wstring& title, const std::wstring& body);
    void Remove();

private:
    void DestroyIcons();

    NOTIFYICONDATAW data_;
    HICON trayIcon_;
    HICON notificationIcon_;
    bool added_;
};

}  // namespace tray_icon
