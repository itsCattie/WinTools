#pragma once

// StreamVault: streaming service manages core logic and state.

#include <QList>
#include <QString>
#include <QVector>

namespace wintools::streamvault {

enum class StreamingService {
    Netflix,
    DisneyPlus,
    Max,
    Hulu,
    PrimeVideo,
    AppleTVPlus,
    Peacock,
    ParamountPlus,
    Crunchyroll,
    Funimation,
    PlexTV,
    Tubi,
    COUNT
};

struct ServiceInfo {
    StreamingService id;
    QString          name;
    QString          accentColor;

    QString          homeUrl;
    QString          loginUrl;
    QString          searchUrlTemplate;

    QList<int>       tmdbProviderIds;
};

inline QVector<ServiceInfo> allServices() {
    return {
        { StreamingService::Netflix,
          "Netflix",          "#E50914",
          "https://www.netflix.com",
          "https://www.netflix.com/login",
          "https://www.netflix.com/search?q=%1",
          {8} },

        { StreamingService::DisneyPlus,
          "Disney+",           "#0063E5",
          "https://www.disneyplus.com",
          "https://www.disneyplus.com/login",
          "https://www.disneyplus.com/search/%1",
          {337} },

        { StreamingService::Max,
          "Max",               "#002BE7",
          "https://www.max.com",
          "https://auth.max.com/",
          "https://www.max.com/search?q=%1",
          {384, 1899} },

        { StreamingService::Hulu,
          "Hulu",              "#1CE783",
          "https://www.hulu.com",
          "https://auth.hulu.com/web/login",
          "https://www.hulu.com/search?q=%1",
          {15} },

        { StreamingService::PrimeVideo,
          "Prime Video",       "#00A8E6",
          "https://www.primevideo.com",
          "https://www.amazon.com/gp/sign-in.html",
          "https://www.primevideo.com/search/ref=atv_nb_sr?phrase=%1",
          {9, 119} },

        { StreamingService::AppleTVPlus,
          "Apple TV+",         "#A2AAAD",
          "https://tv.apple.com",
          "https://tv.apple.com",
          "https://tv.apple.com/search?term=%1",
          {350} },

        { StreamingService::Peacock,
          "Peacock",           "#F4C519",
          "https://www.peacocktv.com",
          "https://www.peacocktv.com/signin",
          "https://www.peacocktv.com/search?q=%1",
          {386, 387} },

        { StreamingService::ParamountPlus,
          "Paramount+",        "#0064FF",
          "https://www.paramountplus.com",
          "https://www.paramountplus.com/account/signin/",
          "https://www.paramountplus.com/search/?query=%1",
          {531} },

        { StreamingService::Crunchyroll,
          "Crunchyroll",       "#F47521",
          "https://www.crunchyroll.com",
          "https://www.crunchyroll.com/login",
          "https://www.crunchyroll.com/search?q=%1",
          {283} },

        { StreamingService::Funimation,
          "Funimation",        "#5B0BB5",
          "https://www.funimation.com",
          "https://www.funimation.com/log-in/",
          "https://www.funimation.com/search/?q=%1",
          {269, 213} },

        { StreamingService::PlexTV,
          "Plex",              "#E5A00D",
          "https://www.plex.tv",
          "https://app.plex.tv/auth/",
          "https://app.plex.tv/desktop/#!/search?query=%1",
          {538} },

        { StreamingService::Tubi,
          "Tubi",              "#FA4E00",
          "https://tubitv.com",
          "https://tubitv.com/login",
          "https://tubitv.com/search/%1",
          {73} },
    };
}

inline const ServiceInfo* findService(StreamingService sid) {
    static const QVector<ServiceInfo> svc = allServices();
    for (const auto& s : svc) {
        if (s.id == sid) return &s;
    }
    return nullptr;
}

inline const ServiceInfo* findServiceByTmdbId(int tmdbProviderId) {
    static const QVector<ServiceInfo> svc = allServices();
    for (const auto& s : svc) {
        if (s.tmdbProviderIds.contains(tmdbProviderId))
            return &s;
    }
    return nullptr;
}

}
