#pragma once

// WinTools: update manages feature behavior.

#include <QString>
#include <QUrl>

class QWidget;

namespace wintools::update {

struct ReleaseInfo {
    bool    success = false;
    bool    updateAvailable = false;
    QString currentVersion;
    QString latestVersion;
    QUrl    releasePageUrl;
    QUrl    downloadUrl;
    QString downloadFileName;
    QString errorMessage;
};

class Update {
public:
    static ReleaseInfo checkForUpdates();
    static bool applyUpdate(const ReleaseInfo& release, QWidget* parent = nullptr);
};

}
