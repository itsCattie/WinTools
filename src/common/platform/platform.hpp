#pragma once

#include <QString>

namespace wintools::platform {

enum class OS { Windows, Linux, macOS, Unknown };

#if defined(Q_OS_WIN) || defined(_WIN32)
inline constexpr OS kCurrentOS = OS::Windows;
#elif defined(Q_OS_LINUX)
inline constexpr OS kCurrentOS = OS::Linux;
#elif defined(Q_OS_MACOS)
inline constexpr OS kCurrentOS = OS::macOS;
#else
inline constexpr OS kCurrentOS = OS::Unknown;
#endif

inline constexpr bool isWindows() { return kCurrentOS == OS::Windows; }

inline constexpr bool isLinux()   { return kCurrentOS == OS::Linux;   }

inline constexpr bool isMacOS()   { return kCurrentOS == OS::macOS;   }

inline QString platformName() {
    switch (kCurrentOS) {
        case OS::Windows: return QStringLiteral("Windows");
        case OS::Linux:   return QStringLiteral("Linux");
        case OS::macOS:   return QStringLiteral("macOS");
        default:          return QStringLiteral("Unknown");
    }
}

}
