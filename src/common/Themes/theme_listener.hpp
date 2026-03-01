#pragma once

#include <QObject>
#include "common/themes/theme_helper.hpp"

class QTimer;

namespace wintools::themes {

class ThemeListener : public QObject {
    Q_OBJECT

public:
    explicit ThemeListener(QObject* parent = nullptr);

signals:
    void themeChanged(bool isDark);

private slots:
    void pollTheme();

private:
    bool m_isDarkTheme;
    ThemeHelper::ThemeMode m_mode;
    QTimer* m_timer;
};

}
