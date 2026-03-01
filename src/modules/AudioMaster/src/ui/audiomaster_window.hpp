#pragma once

#include <QWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <vector>

#include "common/themes/theme_listener.hpp"
#include "common/themes/theme_helper.hpp"

namespace wintools::audiomaster {

class VuMeterWidget;
class VirtualDevicesWidget;

void rotateLinkedOutputDevice();
void rotateLinkedInputDevice();

class AudioMasterWindow : public QWidget {
    Q_OBJECT
public:
    explicit AudioMasterWindow(QWidget* parent = nullptr);
    ~AudioMasterWindow() override;
    void applyTheme(const wintools::themes::ThemePalette& palette);

private slots:
    void refreshDevices();
    void onThemeChanged(bool);

private:
    struct ChannelWidgets {
        QString key;
        QString title;
        bool isInput{false};
        QGroupBox* card{nullptr};
        QLabel* linkedDeviceLabel{nullptr};
        QLabel* valueLabel{nullptr};
        QLabel* appsLabel{nullptr};
        QSlider* slider{nullptr};
        QPushButton* muteButton{nullptr};
        QPushButton* activateButton{nullptr};
        QComboBox* routingCombo{nullptr};
        VuMeterWidget* vuMeter{nullptr};
    };

    struct AppRow {
        QWidget* container{nullptr};
        QLabel* nameLabel{nullptr};
        QSlider* slider{nullptr};
        QLabel* valueLabel{nullptr};
        QPushButton* muteButton{nullptr};
        QString deviceId;
        quint32 pid{0};
    };

    void buildUi();
    void buildMixerPage();
    void buildRoutingPage();
    void buildAppsPage();
    void refreshAppUsage();
    void refreshAppSessions();
    void applyChannelActivation(int channelIndex);
    void refreshCycleLists();
    void refreshRoutingControls();
    void refreshMixerValues();
    void onSliderValueChanged(int channelIndex, int value);
    void onRoutingChanged(int channelIndex);
    QString linkedDeviceId(const ChannelWidgets& channel) const;
    void setLinkedDeviceId(const ChannelWidgets& channel, const QString& deviceId);

    QPushButton* mixerTabButton_{nullptr};
    QPushButton* routingTabButton_{nullptr};
    QPushButton* appsTabButton_{nullptr};
    QPushButton* virtualTabButton_{nullptr};
    QPushButton* refreshButton_{nullptr};
    QLabel* routingHintLabel_{nullptr};
    QListWidget* outputCycleList_{nullptr};
    QListWidget* inputCycleList_{nullptr};
    bool updatingCycleLists_{false};
    QStackedWidget* pages_{nullptr};
    QWidget* mixerPage_{nullptr};
    QWidget* routingPage_{nullptr};
    QWidget* appsPage_{nullptr};
    VirtualDevicesWidget* virtualDevicesWidget_{nullptr};
    QScrollArea* appsScrollArea_{nullptr};
    QWidget* appsScrollContent_{nullptr};
    QVBoxLayout* appsScrollLayout_{nullptr};
    std::vector<AppRow> appRows_;
    std::vector<ChannelWidgets> channels_;
    QSettings settings_{QStringLiteral("wintools"), QStringLiteral("audiomaster")};
    QTimer* usageRefreshTimer_{nullptr};
    QTimer* meterTimer_{nullptr};

    wintools::themes::ThemeListener* m_themeListener_{nullptr};
    wintools::themes::ThemePalette m_palette{};

    void* m_deviceNotifier{nullptr};
    void* m_deviceEnumerator{nullptr};
};

}
