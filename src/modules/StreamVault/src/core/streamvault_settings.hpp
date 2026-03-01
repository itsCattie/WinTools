#pragma once

#include "modules/StreamVault/src/core/streaming_service.hpp"

#include <QString>
#include <QVector>

namespace wintools::streamvault {

class StreamVaultSettings {
public:
    static StreamVaultSettings& instance();

    QString tmdbApiKey() const;
    void    setTmdbApiKey(const QString& key);

    bool    showAdultContent() const;
    void    setShowAdultContent(bool enabled);

    QString defaultLanguage() const;
    void    setDefaultLanguage(const QString& lang);

    QVector<ServiceInfo> customServices() const;
    void                 setCustomServices(const QVector<ServiceInfo>& services);

    static QString posterBaseUrl();
    static QString backdropBaseUrl();

private:
    StreamVaultSettings() = default;
};

}
