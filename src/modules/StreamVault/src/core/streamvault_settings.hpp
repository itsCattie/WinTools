#pragma once

// StreamVault: streamvault settings manages core logic and state.

#include <QString>

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

    static QString posterBaseUrl();
    static QString backdropBaseUrl();

private:
    StreamVaultSettings() = default;
};

}
