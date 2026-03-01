#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QSize>
#include <QString>

#include "common/themes/color_utils.hpp"

namespace wintools::mediabar::gui_detail {

using wintools::themes::tintedIcon;
using wintools::themes::blendColor;
using wintools::themes::compositeOver;
using wintools::themes::readableTextOn;
using wintools::themes::cssRgba;

QIcon drawnShuffleIcon(const QSize& size, const QColor& color);
int lyricItemHeightForText(const QString& text, const QFont& font, int availableWidth);
QColor appForeground();
QColor appMutedForeground();
QColor appAccentColor();

}
