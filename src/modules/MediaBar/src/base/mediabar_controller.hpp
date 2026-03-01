#pragma once

#include <QObject>

class LyricsApp;

namespace wintools::modules {

class MediaBarController : public QObject {
    Q_OBJECT
public:
    static MediaBarController* instance();

    bool isRunning() const;

    void toggle();

    void launchMini();

    void launchFull();

private:
    explicit MediaBarController(QObject* parent = nullptr);

    void ensureStarted();

    LyricsApp* m_app{nullptr};
};

}
