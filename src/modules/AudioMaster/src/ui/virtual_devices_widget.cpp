#include "modules/AudioMaster/src/ui/virtual_devices_widget.hpp"
#include "logger/logger.hpp"
#include <QGridLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QMessageBox>

namespace wintools::audiomaster {

static constexpr const char* LogSource = "AudioMaster/Virtual";

VirtualDevicesWidget::VirtualDevicesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* vgLayout = new QGridLayout(this);

    vbGameCombo_ = new QComboBox(this);
    vbChatCombo_ = new QComboBox(this);
    vbMicCombo_ = new QComboBox(this);

    vbGameBtn_ = new QPushButton(QStringLiteral("Create"), this);
    vbChatBtn_ = new QPushButton(QStringLiteral("Create"), this);
    vbMicBtn_ = new QPushButton(QStringLiteral("Create"), this);
    applyRoutingBtn_ = new QPushButton(QStringLiteral("Save Routing"), this);

    vgLayout->addWidget(new QLabel(QStringLiteral("AudioMasterGame"), this), 0, 0);
    vgLayout->addWidget(vbGameCombo_, 0, 1);
    vgLayout->addWidget(vbGameBtn_, 0, 2);

    vgLayout->addWidget(new QLabel(QStringLiteral("AudioMasterChat"), this), 1, 0);
    vgLayout->addWidget(vbChatCombo_, 1, 1);
    vgLayout->addWidget(vbChatBtn_, 1, 2);

    vgLayout->addWidget(new QLabel(QStringLiteral("AudioMasterMic"), this), 2, 0);
    vgLayout->addWidget(vbMicCombo_, 2, 1);
    vgLayout->addWidget(vbMicBtn_, 2, 2);

    vgLayout->addWidget(applyRoutingBtn_, 3, 0, 1, 3);

    connect(vbGameBtn_, &QPushButton::clicked, this, &VirtualDevicesWidget::onCreateGame);
    connect(vbChatBtn_, &QPushButton::clicked, this, &VirtualDevicesWidget::onCreateChat);
    connect(vbMicBtn_, &QPushButton::clicked, this, &VirtualDevicesWidget::onCreateMic);
    connect(applyRoutingBtn_, &QPushButton::clicked, this, &VirtualDevicesWidget::onApplyRouting);

    populateVirtualDeviceControls();
}

void VirtualDevicesWidget::populateVirtualDeviceControls() {
    vbGameCombo_->clear();
    vbChatCombo_->clear();
    vbMicCombo_->clear();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice& dev : outputs) {
        const QString id = QString::fromUtf8(dev.id());
        vbGameCombo_->addItem(dev.description(), id);
        vbChatCombo_->addItem(dev.description(), id);
        vbMicCombo_->addItem(dev.description(), id);
    }
#endif

    const QString g = m_settings_.value("routing/Game").toString();
    const QString c = m_settings_.value("routing/Chat").toString();
    const QString m = m_settings_.value("routing/Mic").toString();

    auto selectMatching = [](QComboBox* cb, const QString& id) {
        if (!cb) return;
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemData(i).toString() == id) { cb->setCurrentIndex(i); return; }
        }
    };

    selectMatching(vbGameCombo_, g);
    selectMatching(vbChatCombo_, c);
    selectMatching(vbMicCombo_, m);
}

void VirtualDevicesWidget::onCreateGame() {
    const QString name = QStringLiteral("AudioMasterGame");
    const QString msg = QStringLiteral(
        "Creating a virtual audio device requires a virtual audio driver (e.g. VB-Audio Virtual Cable).\n\n"
        "Please install a virtual audio driver and then use the drop-down to route a real output to the virtual device.\n\n"
        "Recommended: https://vb-audio.com/Cable/\n\n"
        "After installation, press Refresh to see the new devices.");
    QMessageBox::information(this, QStringLiteral("Install Virtual Device"), msg);
    wintools::logger::Logger::log(QString::fromUtf8(LogSource), wintools::logger::Severity::Pass, QStringLiteral("User prompted to install virtual device: %1").arg(name));
}

void VirtualDevicesWidget::onCreateChat() { onCreateGame(); }
void VirtualDevicesWidget::onCreateMic() { onCreateGame(); }

void VirtualDevicesWidget::onApplyRouting() {
    const QString gId = vbGameCombo_ ? vbGameCombo_->currentData().toString() : QString();
    const QString cId = vbChatCombo_ ? vbChatCombo_->currentData().toString() : QString();
    const QString mId = vbMicCombo_ ? vbMicCombo_->currentData().toString() : QString();

    m_settings_.setValue("routing/Game", gId);
    m_settings_.setValue("routing/Chat", cId);
    m_settings_.setValue("routing/Mic", mId);
    m_settings_.sync();

    wintools::logger::Logger::log(QString::fromUtf8(LogSource), wintools::logger::Severity::Pass, QStringLiteral("Saved virtual device routing."));
    QMessageBox::information(this, QStringLiteral("Routing Saved"), QStringLiteral("Virtual device routing saved locally. Use an external virtual driver to expose named devices."));
}

}
