#pragma once

#include "common/display/monitors.hpp"
#include "common/hotkey/hotkey_engine.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/themes/window_colour.hpp"
#include "modules/module_provider.hpp"
#include "update/update.hpp"

#include <QMainWindow>

class QLabel;
class QPushButton;
class QGridLayout;
class QTableWidget;
class QGroupBox;
class QComboBox;
class QCheckBox;
class QFrame;

namespace wintools::ui   { class TrayManager;   }
namespace wintools::hotkeys { struct HotkeyAction; }
namespace wintools::streamvault { class ContentNotificationService; }

namespace wintools::ui {

class LaunchPage : public QMainWindow {
    Q_OBJECT

public:
    explicit LaunchPage(QWidget* parent = nullptr);

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onThemeChanged(bool);
    void onHotkeyTriggered(const wintools::hotkeys::HotkeyAction& action);
    void showMainWindow();
    void onHotkeyTableCellClicked(int row, int col);
    void onHotkeyTableContextMenu(const QPoint& pos);
    void checkForUpdates();
    void onUpdateCheckFinished(wintools::update::ReleaseInfo info);
    void onUpdateStatusActivated(const QString& link);

private:
    void applyTheme(const wintools::themes::ThemePalette& palette);
    void rebuildSectionContent();
    void applyUpdateStatusText();
    void dispatchModuleAction(const QString& module, const QString& action);

    void openHotkeyDialog(const QString& module,
                          const QString& actionId,
                          const QString& actionLabel);
    QHash<QString, QString> buildHotkeyDisplayMap() const;
    void restoreHotkeysTablePreferences();
    void saveHotkeysTablePreferences() const;
    void loadGlobalSettings();
    void saveGlobalSettings() const;
    void updateTrayTooltip();

    std::vector<wintools::modules::ModuleEntry> m_modules;
    wintools::themes::ThemeListener*            m_themeListener;
    wintools::display::Monitors                 m_monitors;
    wintools::ui::TrayManager*                  m_tray;
    wintools::hotkeys::HotkeyEngine*            m_hotkeyEngine;

    wintools::themes::ThemePalette m_palette;
    bool    m_isUpToDate;
    bool    m_silentUpdateCheck = false;
    QString m_updateVersion;
    wintools::update::ReleaseInfo m_releaseInfo;

    QLabel*      m_titleLabel;
    QLabel*      m_statusLabel;
    QLabel*      m_statusValueLabel;
    QFrame*      m_topBarFrame;

    QGroupBox*   m_quickAccessCard;
    QGroupBox*   m_hotkeysCard;
    QGroupBox*   m_modulesCard;
    QGroupBox*   m_settingsCard;

    QGridLayout*  m_quickGrid;
    QTableWidget* m_hotkeysTable;
    QTableWidget* m_modulesTable;
    QComboBox*    m_themeModeCombo;
    QCheckBox*    m_closeToTrayCheck;
    QCheckBox*    m_autoUpdateCheck;

    wintools::streamvault::ContentNotificationService* m_notifService = nullptr;
};

}
