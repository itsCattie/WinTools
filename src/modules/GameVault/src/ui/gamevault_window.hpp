#pragma once

// GameVault: gamevault window manages UI behavior and presentation.

#include "modules/GameVault/src/core/game_entry.hpp"
#include "common/themes/window_colour.hpp"

#include <QDialog>
#include <QHash>
#include <QListWidgetItem>
#include <QPixmap>
#include <QVector>

class QFrame;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QPushButton;
class QProgressBar;
class QScrollArea;
class QStackedWidget;
class QTextBrowser;
class QNetworkAccessManager;
class QNetworkReply;

namespace wintools::themes { class ThemeListener; }

namespace wintools::gamevault {

class GameListModel;
class GameFilterProxy;
class GameCardDelegate;

class GameVaultWindow : public QDialog {
    Q_OBJECT

public:
    explicit GameVaultWindow(QWidget* parent = nullptr);
    ~GameVaultWindow() override;

private slots:
    void onThemeChanged(bool);
    void onScanComplete(QVector<wintools::gamevault::GameEntry> games);
    void onScanError(QString message);
    void onSearchChanged(const QString& text);
    void onSidebarItemClicked(int row);
    void onInstalledOnlyToggled(bool checked);
    void onGridActivated(const QModelIndex& idx);
    void onGridContextMenu(const QPoint& pos);
    void onBannerLoaded();
    void onCardArtLoaded();
    void rescan();
    void openSettings();
    void goToDetail(const GameEntry& e);
    void goToLibrary();
    void launchEntry(const GameEntry& e);
    void addCustomGame();

private:

    void buildUi();
    QWidget* buildSidebar();
    QWidget* buildLibraryPage();
    QWidget* buildDetailPage();
    void startScan();
    void updateSidebarCounts();
    void rebuildArtQueue();
    void fetchNextCardArt();
    void fetchDetailBanner(const QString& url);
    void setStatusText(const QString& text);
    void applyTheme(const wintools::themes::ThemePalette& palette);

    QListWidget*      m_sidebar         = nullptr;
    QListWidgetItem*  m_allGamesItem    = nullptr;
    QHash<int, QListWidgetItem*> m_platformItems;
    QPushButton*      m_installedToggle = nullptr;
    QPushButton*      m_settingsBtn     = nullptr;

    QLineEdit*        m_search          = nullptr;
    QPushButton*      m_rescanBtn       = nullptr;
    QLabel*           m_statusLabel     = nullptr;
    QListView*        m_gridView        = nullptr;
    GameCardDelegate* m_cardDelegate    = nullptr;

    QLabel*           m_heroLabel       = nullptr;
    QLabel*           m_heroTitle       = nullptr;
    QLabel*           m_detailPlatform  = nullptr;
    QLabel*           m_detailPlaytime  = nullptr;
    QLabel*           m_detailLastPlay  = nullptr;
    QFrame*           m_achSection      = nullptr;
    QLabel*           m_achLabel        = nullptr;
    QProgressBar*     m_achBar          = nullptr;
    QTextBrowser*     m_newsBrowser     = nullptr;
    QPushButton*      m_playBtn         = nullptr;
    QPushButton*      m_folderBtn       = nullptr;
    QPushButton*      m_customArtBtn    = nullptr;
    GameEntry         m_currentEntry;

    QStackedWidget*   m_stack           = nullptr;
    static constexpr int kLibraryPage   = 0;
    static constexpr int kDetailPage    = 1;

    GameListModel*    m_model           = nullptr;
    GameFilterProxy*  m_proxy           = nullptr;
    bool              m_installedOnly   = false;
    int               m_activePlatform  = -1;

    QHash<QString, QPixmap> m_artCache;
    QStringList             m_artQueue;
    QNetworkAccessManager*  m_nam         = nullptr;
    QNetworkReply*          m_cardReply   = nullptr;
    QNetworkReply*          m_bannerReply = nullptr;
    QNetworkReply*          m_newsReply   = nullptr;
    QString                 m_cardReplyUrl;

    wintools::themes::ThemeListener* m_themeListener = nullptr;
    wintools::themes::ThemePalette   m_palette;
};

}
