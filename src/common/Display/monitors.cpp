#include "common/display/monitors.hpp"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include <algorithm>

// WinTools: monitors manages shared infrastructure.

namespace wintools::display {

namespace {
QRect currentWorkArea(QWidget* window) {
    if (!window) {
        if (const QScreen* screen = QGuiApplication::primaryScreen()) {
            return screen->availableGeometry();
        }
        return {};
    }

    const QPoint center = window->frameGeometry().center();
    if (QScreen* screen = QGuiApplication::screenAt(center)) {
        return screen->availableGeometry();
    }

    if (QScreen* screen = window->screen()) {
        return screen->availableGeometry();
    }

    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        return screen->availableGeometry();
    }

    return {};
}
}

Monitors::Monitors(QWidget* window)
    : m_workingArea(currentWorkArea(window)) {}

bool Monitors::track(QWidget* window) {
    const QRect updated = currentWorkArea(window);
    if (updated == m_workingArea) {
        return false;
    }

    m_workingArea = updated;
    ensureOnScreen(window);
    return true;
}

void Monitors::ensureOnScreen(QWidget* window) {
    if (!window) {
        return;
    }

    const QRect work = currentWorkArea(window);
    const QRect frame = window->frameGeometry();

    const int xMin = work.left();
    const int yMin = work.top();
    const int xMax = std::max(work.left(), work.right() - frame.width());
    const int yMax = std::max(work.top(), work.bottom() - frame.height());

    const int x = std::clamp(window->x(), xMin, xMax);
    const int y = std::clamp(window->y(), yMin, yMax);

    window->move(x, y);
}

}
