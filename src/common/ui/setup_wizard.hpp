#pragma once

#include <QWizard>

class QComboBox;
class QCheckBox;
class QLabel;

namespace wintools::hotkeys { class HotkeyEngine; }

namespace wintools::ui {

class SetupWizard : public QWizard {
    Q_OBJECT
public:
    explicit SetupWizard(wintools::hotkeys::HotkeyEngine* engine,
                          QWidget* parent = nullptr);

    static bool hasCompletedSetup();

    static void markSetupComplete();

private:
    void createWelcomePage();
    void createThemePage();
    void createBehaviourPage();
    void createFinishPage();
    void applySelections();

    QComboBox*  m_themeCombo{nullptr};
    QCheckBox*  m_closeToTray{nullptr};
    QCheckBox*  m_autoUpdate{nullptr};
    QCheckBox*  m_startWithWindows{nullptr};

    wintools::hotkeys::HotkeyEngine* m_engine{nullptr};
};

}
