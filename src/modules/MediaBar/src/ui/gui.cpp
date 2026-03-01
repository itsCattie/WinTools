#include "gui.hpp"

#include "config.hpp"

#include <windows.h>

#include <QKeyEvent>
#include <QSlider>
#include <common/Themes/theme_helper.hpp>

LyricsWindow::LyricsWindow(QWidget* parent)
		: QWidget(parent) {
		setupUi();

		themeListener_ = new wintools::themes::ThemeListener(this);
		connect(themeListener_, &wintools::themes::ThemeListener::themeChanged,
				this, &LyricsWindow::onThemeChanged);
	}

void LyricsWindow::onThemeChanged(bool ) {

	setupUi();

	updateShuffleButtonStyle();

	updateTransportIcons();
}

bool LyricsWindow::nativeEvent(const QByteArray& eventType,
								void* message, qintptr* result) {
	if (eventType == "windows_generic_MSG") {
		static const UINT ipcMsg =
			RegisterWindowMessage(LyricsWindow::IpcMsgName);
		const auto* msg = static_cast<const MSG*>(message);
		if (ipcMsg && msg->message == ipcMsg) {
			emit ipcCommand(static_cast<int>(msg->wParam));
			*result = 0;
			return true;
		}
	}
	return QWidget::nativeEvent(eventType, message, result);
}
void LyricsWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Space:
        emit playPauseRequested();
        event->accept();
        return;
    case Qt::Key_Left:
        emit prevRequested();
        event->accept();
        return;
    case Qt::Key_Right:
        emit nextRequested();
        event->accept();
        return;
    case Qt::Key_Up:
        if (volumeSlider_ && volumeSlider_->isEnabled()) {
            const int newVal = qMin(volumeSlider_->maximum(), volumeSlider_->value() + 5);
            volumeSlider_->setValue(newVal);
            emit volumeChanged(newVal);
        }
        event->accept();
        return;
    case Qt::Key_Down:
        if (volumeSlider_ && volumeSlider_->isEnabled()) {
            const int newVal = qMax(volumeSlider_->minimum(), volumeSlider_->value() - 5);
            volumeSlider_->setValue(newVal);
            emit volumeChanged(newVal);
        }
        event->accept();
        return;
    case Qt::Key_S:
        emit shuffleRequested();
        event->accept();
        return;
    case Qt::Key_R:
        emit repeatRequested();
        event->accept();
        return;
    case Qt::Key_M:
        emit miniModeRequested();
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}
