#include "common/themes/theme_listener.hpp"

#include "common/themes/theme_helper.hpp"

#include <QTimer>

namespace wintools::themes {

ThemeListener::ThemeListener(QObject* parent)
        : QObject(parent),
            m_isDarkTheme(ThemeHelper::isDarkTheme()),
            m_mode(ThemeHelper::themeMode()),
            m_timer(new QTimer(this)) {
    connect(m_timer, &QTimer::timeout, this, &ThemeListener::pollTheme);
    m_timer->start(1000);

        connect(ThemeNotifier::instance(), &ThemeNotifier::themeChanged,
                        this, [this](int newMode) {

                                if (newMode == static_cast<int>(m_mode)) return;
                                m_mode = static_cast<ThemeHelper::ThemeMode>(newMode);

                                m_isDarkTheme = ThemeHelper::isDarkTheme();
                                emit themeChanged(m_isDarkTheme);
                        });
}

void ThemeListener::pollTheme() {

    const ThemeHelper::ThemeMode currentMode = ThemeHelper::themeMode();
    const bool currentIsDark = ThemeHelper::isDarkTheme();
    if (currentMode == m_mode && currentIsDark == m_isDarkTheme) {
        return;
    }

    m_mode = currentMode;
    m_isDarkTheme = currentIsDark;
    emit themeChanged(m_isDarkTheme);
}

}
