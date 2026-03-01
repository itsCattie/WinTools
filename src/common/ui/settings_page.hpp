#pragma once

#include <QDialog>

class QComboBox;
class QCheckBox;
class QTableWidget;
class QPushButton;
class QTabWidget;

namespace wintools::themes { struct ThemePalette; class ThemeListener; }
namespace wintools::hotkeys { class HotkeyEngine; struct HotkeyBinding; }

namespace wintools::ui {

class SettingsPage : public QDialog {
    Q_OBJECT
public:
    explicit SettingsPage(wintools::hotkeys::HotkeyEngine* engine,
                          QWidget* parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void onThemeChanged(int index);
    void onCloseToTrayToggled(bool checked);
    void onAutoUpdateToggled(bool checked);
    void onStartWithWindowsToggled(bool checked);
    void rebindHotkey(int row);
    void resetHotkeysToDefaults();

private:
    void buildGeneralTab(QWidget* tab);
    void buildHotkeysTab(QWidget* tab);
    void buildAboutTab(QWidget* tab);
    void loadSettings();
    void populateHotkeyTable();
    void applyTheme(const wintools::themes::ThemePalette& palette);

    QTabWidget*   m_tabs{nullptr};
    QComboBox*    m_themeCombo{nullptr};
    QCheckBox*    m_closeToTray{nullptr};
    QCheckBox*    m_autoUpdate{nullptr};
    QCheckBox*    m_startWithWindows{nullptr};
    QTableWidget* m_hotkeyTable{nullptr};
    QPushButton*  m_resetHotkeys{nullptr};

    wintools::hotkeys::HotkeyEngine* m_hotkeyEngine{nullptr};
    wintools::themes::ThemeListener* m_themeListener{nullptr};
};

}
