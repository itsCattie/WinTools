#include "common/ui/screen_relative_size.hpp"

#include <QScreen>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#include <cmath>

namespace wintools::ui {

namespace {

constexpr const char* kPrevAvailSizeProp = "_wintools_prev_available_size";
constexpr const char* kScreenTrackingConnectedProp = "_wintools_screen_tracking_connected";

QSize clampedScaledSize(const QWidget* widget,
                        const QSize& current,
                        const QSize& oldAvailable,
                        const QSize& newAvailable) {
    if (!widget || !current.isValid() || !oldAvailable.isValid() || !newAvailable.isValid()) {
        return current;
    }

    const double scaleX = static_cast<double>(newAvailable.width())
                        / static_cast<double>(oldAvailable.width());
    const double scaleY = static_cast<double>(newAvailable.height())
                        / static_cast<double>(oldAvailable.height());

    QSize scaled(static_cast<int>(std::lround(current.width() * scaleX)),
                 static_cast<int>(std::lround(current.height() * scaleY)));

    const QSize min = widget->minimumSize();
    scaled.setWidth(std::max(scaled.width(), min.width()));
    scaled.setHeight(std::max(scaled.height(), min.height()));

    const QSize max = widget->maximumSize();
    if (max.width() >= 0) {
        scaled.setWidth(std::min(scaled.width(), max.width()));
    }
    if (max.height() >= 0) {
        scaled.setHeight(std::min(scaled.height(), max.height()));
    }

    return scaled;
}

void setTrackedAvailableSize(QWidget* widget, QScreen* screen) {
    if (!widget || !screen) {
        return;
    }
    widget->setProperty(kPrevAvailSizeProp, screen->availableGeometry().size());
}

}

void enableRelativeSizeAcrossScreens(QWidget* widget) {
    if (!widget) {
        return;
    }

    if (widget->property(kScreenTrackingConnectedProp).toBool()) {
        return;
    }

    widget->winId();
    QWindow* window = widget->windowHandle();
    if (!window) {
        return;
    }

    setTrackedAvailableSize(widget, window->screen());

    QObject::connect(window, &QWindow::screenChanged, widget,
                     [widget](QScreen* newScreen) {
        if (!widget || !newScreen) {
            return;
        }

        const QSize oldAvailable = widget->property(kPrevAvailSizeProp).toSize();
        const QSize newAvailable = newScreen->availableGeometry().size();

        if (oldAvailable.isValid() && newAvailable.isValid() && oldAvailable != newAvailable) {
            const QSize target = clampedScaledSize(widget, widget->size(), oldAvailable, newAvailable);
            if (target != widget->size()) {
                widget->resize(target);
            }
        }

        widget->setProperty(kPrevAvailSizeProp, newAvailable);
    });

    widget->setProperty(kScreenTrackingConnectedProp, true);
}

}
