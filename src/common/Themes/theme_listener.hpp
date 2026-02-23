#pragma once

// WinTools: theme listener manages shared infrastructure.

#include <QObject>

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
    QTimer* m_timer;
};

}
