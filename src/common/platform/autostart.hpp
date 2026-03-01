#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace wintools::platform {

#if defined(Q_OS_WIN)

namespace detail {
inline const wchar_t* kRunKey  = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
inline const wchar_t* kAppName = L"WinTools";
}

inline bool isStartWithWindows() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, detail::kRunKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    DWORD type = 0;
    DWORD size = 0;
    bool exists = (RegQueryValueExW(hKey, detail::kAppName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

inline void setStartWithWindows(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, detail::kRunKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        auto len = static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, detail::kAppName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(exePath), len);
    } else {
        RegDeleteValueW(hKey, detail::kAppName);
    }
    RegCloseKey(hKey);
}

#else

inline bool isStartWithWindows() { return false; }
inline void setStartWithWindows(bool) {}

#endif

}
