#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace wintools::audiomaster {

struct AudioDevice {
    QString id;
    QString name;
    bool isInput{false};
    bool isDefault{false};
};

struct AudioAppSession {
    QString deviceId;
    quint32 pid{0};
    QString processName;
    float volume{1.0f};
    bool muted{false};
};

class AudioBackend : public QObject {
    Q_OBJECT
public:
    explicit AudioBackend(QObject* parent = nullptr) : QObject(parent) {}
    ~AudioBackend() override = default;

    static AudioBackend* create(QObject* parent = nullptr);

    virtual QVector<AudioDevice> enumerateDevices() { return {}; }

    virtual bool setDefaultEndpoint(const QString& ) { return false; }

    virtual float getEndpointVolume(const QString& ) { return 0.0f; }
    virtual bool  setEndpointVolume(const QString& , float ) { return false; }
    virtual bool  getEndpointMute(const QString& ) { return false; }
    virtual bool  setEndpointMute(const QString& , bool ) { return false; }

    virtual float getEndpointPeak(const QString& ) { return 0.0f; }

    virtual QVector<AudioAppSession> enumerateSessions() { return {}; }

    virtual bool setSessionVolume(const QString& , quint32 , float ) { return false; }
    virtual bool setSessionMute(const QString& , quint32 , bool ) { return false; }

    virtual void startDeviceWatcher() {}

    virtual void stopDeviceWatcher() {}

signals:

    void devicesChanged();
};

}
