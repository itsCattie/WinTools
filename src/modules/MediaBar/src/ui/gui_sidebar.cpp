#include "gui.hpp"

#include "config.hpp"
#include "internal/gui_detail_helpers.hpp"

#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include "../core/debug_logger.hpp"

using namespace wintools::mediabar::gui_detail;

void LyricsWindow::setSourceMode(const QString& mode) {
    if (!sourceModeCombo_) {
        return;
    }

    const QString normalized = mode.trimmed().toLower();
    for (int i = 0; i < sourceModeCombo_->count(); ++i) {
        if (sourceModeCombo_->itemData(i).toString() == normalized) {
            sourceModeCombo_->setCurrentIndex(i);
            syncSourceButtons();
            return;
        }
    }

    sourceModeCombo_->setCurrentIndex(0);
    syncSourceButtons();
}

QString LyricsWindow::sourceMode() const {
    if (!sourceModeCombo_) {
        return "spotify";
    }
    return sourceModeCombo_->currentData().toString();
}

void LyricsWindow::toggleSidebar() {
    setSidebarExpanded(!sidebarExpanded_);
}

void LyricsWindow::setSidebarExpanded(bool expanded) {
    if (!sideMenu_) {
        return;
    }

    sidebarExpanded_ = expanded;
    if (!sidebarExpanded_) {
        collapsedVolumePanelVisible_ = false;
    }
    updateSidebarPresentation();

    auto settings = config::loadSettings();
    settings.insert("sidebar_show_labels", sidebarExpanded_);
    config::saveSettings(settings);
}

void LyricsWindow::syncSourceButtons() {
    if (!sourceSpotifyButton_ || !sourceSonosButton_) {
        return;
    }

    const QString mode = sourceMode();
    sourceSpotifyButton_->setChecked(mode == "spotify");
    sourceSonosButton_->setChecked(mode == "sonos");
}

void LyricsWindow::updateSidebarPresentation() {
    if (!sideMenu_) {
        return;
    }

    const int expandedWidth = config::SIDEBAR_EXPANDED_WIDTH;
    const int collapsedWidth = config::SIDEBAR_COLLAPSED_WIDTH;
    sideMenu_->setFixedWidth(sidebarExpanded_ ? expandedWidth : collapsedWidth);

    const auto sideButtons = sideMenu_->findChildren<QPushButton*>();
    for (auto* button : sideButtons) {
        const QString fullText = button->property("fullText").toString();
        if (fullText.isEmpty()) {
            continue;
        }

        if (sidebarExpanded_) {
            button->setText(fullText);
            button->setToolTip(QString());
        } else {
            button->setText("");
            button->setToolTip(fullText);
        }

        if (sidebarExpanded_) {
            button->setIconSize(QSize(22, 22));
            button->setFixedWidth(expandedWidth - 20);
        } else {
            button->setIconSize(QSize(18, 18));
            button->setFixedWidth(collapsedWidth);
        }

        QIcon baseIcon;
        const QVariant v = button->property("origIcon");
        if (v.isValid() && v.canConvert<QIcon>()) {
            baseIcon = qvariant_cast<QIcon>(v);
        } else {
            baseIcon = button->icon();
        }
        if (!baseIcon.isNull()) {
            QColor ic;
            const QColor accent = appAccentColor();
            const QColor normalIcon = appForeground();
            ic = button->isChecked() ? accent : normalIcon;

            button->setIcon(tintedIcon(baseIcon, QSize(22,22), ic));
        } else {
            debuglog::warn("GUI", QString("updateSidebarPresentation: no base icon for '%1' - using existing icon").arg(fullText));
        }
    }

    const auto sectionLabels = sideMenu_->findChildren<QLabel*>();
    for (auto* label : sectionLabels) {
        if (label->text() == "MediaBar") {
            label->setVisible(sidebarExpanded_);
            continue;
        }
        if (label->objectName() == "section") {
            label->setVisible(sidebarExpanded_);
        }
    }

    if (auto* layout = sideMenu_->layout()) {
        if (!sidebarExpanded_) {
            layout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

            layout->setContentsMargins(2, 8, 8, 10);
            layout->setSpacing(6);
            for (auto* button : sideButtons) {
                layout->setAlignment(button, Qt::AlignVCenter | Qt::AlignLeft);
            }
            for (auto* lbl : sectionLabels) {
                layout->setAlignment(lbl, Qt::AlignVCenter | Qt::AlignLeft);
            }

            if (sourceSpotifyButton_) layout->setAlignment(sourceSpotifyButton_, Qt::AlignVCenter | Qt::AlignLeft);
            if (sourceSonosButton_) layout->setAlignment(sourceSonosButton_, Qt::AlignVCenter | Qt::AlignLeft);
        } else {
            layout->setAlignment(Qt::AlignTop);
            layout->setContentsMargins(0, 0, 0, 0);
            for (auto* button : sideButtons) {
                layout->setAlignment(button, Qt::AlignLeft);
            }
            for (auto* lbl : sectionLabels) {
                layout->setAlignment(lbl, Qt::AlignLeft);
            }
            if (sourceSpotifyButton_) layout->setAlignment(sourceSpotifyButton_, Qt::AlignLeft);
            if (sourceSonosButton_) layout->setAlignment(sourceSonosButton_, Qt::AlignLeft);
        }
    }

    sourceSpotifyButton_->setVisible(true);
    sourceSonosButton_->setVisible(true);

    if (volumePanel_ && volumeSlider_) {
        if (auto* volumeLayout = qobject_cast<QBoxLayout*>(volumePanel_->layout())) {
            if (sidebarExpanded_) {
                volumeLayout->setDirection(QBoxLayout::LeftToRight);
                volumePanel_->setFixedHeight(44);
                volumePanel_->setMinimumWidth(std::max(120, expandedWidth - 18));
                volumePanel_->setMaximumWidth(expandedWidth - 8);
                volumeSlider_->setOrientation(Qt::Horizontal);
                volumeSlider_->setInvertedAppearance(false);
                volumeSlider_->setFixedWidth(std::max(80, expandedWidth - 78));
                volumeSlider_->setFixedHeight(18);
                if (volumeGlyphLabel_) {
                    volumeGlyphLabel_->show();
                }
                if (volumeToggleButton_) {
                    volumeToggleButton_->hide();
                }
                volumePanel_->setVisible(true);
            } else {
                volumeLayout->setDirection(QBoxLayout::TopToBottom);
                volumePanel_->setFixedWidth(34);
                volumePanel_->setFixedHeight(120);
                volumeSlider_->setOrientation(Qt::Vertical);
                volumeSlider_->setInvertedAppearance(false);
                volumeSlider_->setFixedWidth(18);
                volumeSlider_->setFixedHeight(86);
                if (volumeGlyphLabel_) {
                    volumeGlyphLabel_->hide();
                }
                if (volumeToggleButton_) {
                    volumeToggleButton_->show();
                    volumeToggleButton_->setFixedWidth(34);
                    volumeToggleButton_->setFixedHeight(32);
                    if (auto* layout = sideMenu_->layout()) {
                        layout->setAlignment(volumeToggleButton_, Qt::AlignHCenter | Qt::AlignBottom);
                        layout->setAlignment(volumePanel_, Qt::AlignHCenter | Qt::AlignBottom);
                    }
                }
                const bool showCollapsedPanel = collapsedVolumePanelVisible_ && volumeSlider_->isEnabled();
                volumePanel_->setVisible(showCollapsedPanel);
            }
        }
    }
}
