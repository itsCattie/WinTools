#include "common/themes/theme_listener.hpp"

#include "common/themes/theme_helper.hpp"

#include <QTimer>

// WinTools: theme listener manages shared infrastructure.

namespace wintools::themes {

ThemeListener::ThemeListener(QObject* parent)
    : QObject(parent),
      m_isDarkTheme(ThemeHelper::isSystemDarkTheme()),
      m_timer(new QTimer(this)) {
    connect(m_timer, &QTimer::timeout, this, &ThemeListener::pollTheme);
    m_timer->start(1000);
}

void ThemeListener::pollTheme() {
    const bool updated = ThemeHelper::isSystemDarkTheme();
    if (updated == m_isDarkTheme) {
        return;
    }

    m_isDarkTheme = updated;
    emit themeChanged(m_isDarkTheme);
}

}
