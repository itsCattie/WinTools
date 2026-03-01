#pragma once

#include <QRect>

class QWidget;

namespace wintools::display {

class Monitors {
public:
    explicit Monitors(QWidget* window);
    bool track(QWidget* window);
    static void ensureOnScreen(QWidget* window);

private:
    QRect m_workingArea;
};

}
