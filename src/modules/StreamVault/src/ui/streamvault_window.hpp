#pragma once

// StreamVault: streamvault window manages UI behavior and presentation.

#include "modules/StreamVault/src/core/stream_entry.hpp"
#include "modules/StreamVault/src/core/streaming_service.hpp"
#include "modules/StreamVault/src/core/tmdb_client.hpp"
#include "common/themes/window_colour.hpp"

#include <QDialog>
#include <QHash>
#include <QMap>
#include <QPixmap>
#include <QVector>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace wintools::themes { class ThemeListener; }

namespace wintools::streamvault {

class StreamListModel;
class StreamFilterProxy;
class StreamCardDelegate;
class TmdbClient;

class StreamVaultWindow : public QDialog {
    Q_OBJECT

public:
    explicit StreamVaultWindow(QWidget* parent = nullptr);
    ~StreamVaultWindow() override;

private slots:
    void onSearchTriggered();
    void onSearchResults(QVector<StreamEntry> results);
    void onSearchError(QString message);
    void onPosterLoaded(int tmdbId, QByteArray imageData);
    void onWatchProvidersLoaded(int tmdbId, WatchProviderMap byCountry);
    void onCountryChanged(int index);
    void onGridActivated(const QModelIndex& idx);
    void onGridContextMenu(const QPoint& pos);
    void goToDetail(const StreamEntry& e);
    void goToSearch();
    void openSettings();
    void onThemeChanged(bool isDark);

private:

    void buildUi();
    void applyTheme(const wintools::themes::ThemePalette& palette);
    QWidget* buildSidebar();
    QWidget* buildSearchPage();
    QWidget* buildDetailPage();

    void populateDetail(const StreamEntry& e);
    void clearDetailServiceButtons();
    void populateServiceButtons(const QString& countryCode);
    void addKnownServiceButton(const ServiceInfo& svc, const QString& titleEncoded);
    void addUnknownProviderButton(const ProviderEntry& pe, const QString& titleEncoded);

    void requestVisiblePosters();
    void fetchBackdrop(const StreamEntry& e);

    static void openUrl(const QString& url);

    QLineEdit*       m_searchEdit    = nullptr;
    QLabel*          m_statusLabel   = nullptr;
    QListView*       m_grid          = nullptr;
    QWidget*         m_filterBar     = nullptr;

    QLabel*          m_backdropLabel = nullptr;
    QLabel*          m_detailTitle   = nullptr;
    QLabel*          m_detailMeta    = nullptr;
    QLabel*          m_detailOverview = nullptr;
    QLabel*          m_watchStatusLabel = nullptr;
    QComboBox*       m_countryCombo  = nullptr;
    QWidget*         m_serviceButtonContainer = nullptr;

    QStackedWidget*  m_stack         = nullptr;
    QListWidget*     m_sidebar       = nullptr;

    StreamListModel* m_model         = nullptr;
    StreamFilterProxy* m_proxy       = nullptr;
    StreamCardDelegate* m_delegate   = nullptr;

    TmdbClient*      m_tmdb          = nullptr;
    QNetworkAccessManager* m_imgNam  = nullptr;

    QHash<int, QPixmap>     m_posterCache;
    QHash<int, QPixmap>     m_backdropCache;

    QHash<int, WatchProviderMap> m_providersCache;

    QTimer*          m_debounce      = nullptr;

    StreamEntry      m_currentEntry;

    wintools::themes::ThemeListener* m_themeListener = nullptr;
    wintools::themes::ThemePalette   m_palette;
};

}
