#pragma once

// WinTools: screen relative size manages UI behavior and presentation.

class QWidget;

namespace wintools::ui {

void enableRelativeSizeAcrossScreens(QWidget* widget);

}
