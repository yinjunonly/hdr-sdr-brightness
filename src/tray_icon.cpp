#include "tray_icon.h"

#include <cwchar>

#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP 0x00000080
#endif
#ifndef NIIF_USER
#define NIIF_USER 0x00000004
#endif
#ifndef NIIF_LARGE_ICON
#define NIIF_LARGE_ICON 0x00000020
#endif

namespace tray_icon {
namespace {

template <size_t Size>
void CopyString(wchar_t (&buffer)[Size], const std::wstring& value) {
    std::wcsncpy(buffer, value.c_str(), Size - 1);
    buffer[Size - 1] = L'\0';
}

}  // namespace

TrayIcon::TrayIcon()
    : data_(), trayIcon_(NULL), notificationIcon_(NULL), added_(false) {}

TrayIcon::~TrayIcon() {
    Remove();
}

void TrayIcon::Add(HINSTANCE instance, HWND window, UINT callbackMessage, int iconResourceId,
                   const std::wstring& tooltip) {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
    DestroyIcons();

    ZeroMemory(&data_, sizeof(data_));
    data_.cbSize = sizeof(data_);
    data_.hWnd = window;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;

    int smallW = GetSystemMetrics(SM_CXSMICON);
    int smallH = GetSystemMetrics(SM_CYSMICON);
    int largeW = GetSystemMetrics(SM_CXICON);
    int largeH = GetSystemMetrics(SM_CYICON);
    trayIcon_ = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(iconResourceId),
                                                   IMAGE_ICON, smallW, smallH, LR_DEFAULTCOLOR));
    notificationIcon_ = reinterpret_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(iconResourceId), IMAGE_ICON, largeW, largeH, LR_DEFAULTCOLOR));

    if (!trayIcon_) {
        HICON fallback = LoadIconW(instance, MAKEINTRESOURCEW(iconResourceId));
        if (!fallback) fallback = LoadIconW(NULL, IDI_APPLICATION);
        trayIcon_ = fallback ? CopyIcon(fallback) : NULL;
    }
    if (!notificationIcon_) {
        HICON fallback = LoadIconW(instance, MAKEINTRESOURCEW(iconResourceId));
        if (!fallback) fallback = LoadIconW(NULL, IDI_APPLICATION);
        notificationIcon_ = fallback ? CopyIcon(fallback) : NULL;
    }

    data_.hIcon = trayIcon_ ? trayIcon_ : LoadIconW(NULL, IDI_APPLICATION);
    CopyString(data_.szTip, tooltip);
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
}

void TrayIcon::UpdateTip(const std::wstring& tooltip) {
    if (!added_) return;
    data_.uFlags = NIF_TIP | NIF_SHOWTIP;
    CopyString(data_.szTip, tooltip);
    Shell_NotifyIconW(NIM_MODIFY, &data_);
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void TrayIcon::ShowNotification(const std::wstring& title, const std::wstring& body) {
    if (!added_) return;
    data_.uFlags = NIF_INFO;
    CopyString(data_.szInfoTitle, title);
    CopyString(data_.szInfo, body);
    data_.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    data_.hBalloonIcon = notificationIcon_ ? notificationIcon_ : data_.hIcon;
    data_.uTimeout = 5000;
    Shell_NotifyIconW(NIM_MODIFY, &data_);
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void TrayIcon::Remove() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
    DestroyIcons();
    ZeroMemory(&data_, sizeof(data_));
}

void TrayIcon::DestroyIcons() {
    if (notificationIcon_ && notificationIcon_ != trayIcon_) DestroyIcon(notificationIcon_);
    if (trayIcon_) DestroyIcon(trayIcon_);
    notificationIcon_ = NULL;
    trayIcon_ = NULL;
}

}  // namespace tray_icon
