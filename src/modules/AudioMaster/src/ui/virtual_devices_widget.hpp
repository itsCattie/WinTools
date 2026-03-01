#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>

namespace wintools::audiomaster {

class VirtualDevicesWidget : public QWidget {
    Q_OBJECT
public:
    explicit VirtualDevicesWidget(QWidget* parent = nullptr);
    void populateVirtualDeviceControls();

private slots:
    void onCreateGame();
    void onCreateChat();
    void onCreateMic();
    void onApplyRouting();

private:
    QComboBox* vbGameCombo_{nullptr};
    QComboBox* vbChatCombo_{nullptr};
    QComboBox* vbMicCombo_{nullptr};
    QPushButton* vbGameBtn_{nullptr};
    QPushButton* vbChatBtn_{nullptr};
    QPushButton* vbMicBtn_{nullptr};
    QPushButton* applyRoutingBtn_{nullptr};

    QSettings m_settings_{"wintools","audiomaster"};
};

}
