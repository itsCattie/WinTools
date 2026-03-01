#include "common/ui/setup_wizard.hpp"
#include "common/platform/autostart.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/hotkey/hotkey_engine.hpp"
#include "wintools_version.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>
#include <QWizardPage>
#include <QIcon>

namespace wintools::ui {

using namespace wintools::themes;
using wintools::platform::setStartWithWindows;

SetupWizard::SetupWizard(wintools::hotkeys::HotkeyEngine* engine, QWidget* parent)
    : QWizard(parent), m_engine(engine) {
    setWindowTitle(QStringLiteral("WinTools Setup"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app/wintools.svg")));
    setMinimumSize(520, 420);
    setWizardStyle(QWizard::ModernStyle);

    createWelcomePage();
    createThemePage();
    createBehaviourPage();
    createFinishPage();

    connect(this, &QWizard::accepted, this, [this]() {
        applySelections();
        markSetupComplete();
    });
}

bool SetupWizard::hasCompletedSetup() {
    QSettings s;
    return s.value(QStringLiteral("ui/global/setup_complete"), false).toBool();
}

void SetupWizard::markSetupComplete() {
    QSettings s;
    s.setValue(QStringLiteral("ui/global/setup_complete"), true);
}

void SetupWizard::createWelcomePage() {
    auto* page = new QWizardPage(this);
    page->setTitle(QStringLiteral("Welcome to WinTools"));
    page->setSubTitle(QStringLiteral("This wizard will help you configure key preferences."));

    auto* layout = new QVBoxLayout(page);

    auto* icon = new QLabel(page);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/app/wintools.svg")).pixmap(96, 96));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto* desc = new QLabel(
        QStringLiteral("<p style='font-size:13px;'>WinTools v%1 is a modern Windows utility suite "
                       "combining media control, audio mixing, disk analysis, task management, "
                       "game library, and streaming tools in one place.</p>"
                       "<p>Click <b>Next</b> to get started.</p>")
            .arg(QStringLiteral(WINTOOLS_APP_VERSION_LITERAL)), page);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    layout->addWidget(desc);
    layout->addStretch();

    addPage(page);
}

void SetupWizard::createThemePage() {
    auto* page = new QWizardPage(this);
    page->setTitle(QStringLiteral("Choose a Theme"));
    page->setSubTitle(QStringLiteral("Select the visual theme for WinTools."));

    auto* layout = new QVBoxLayout(page);

    m_themeCombo = new QComboBox(page);
    ThemeHelper::populateThemeCombo(m_themeCombo);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index < 0) return;
        auto mode = static_cast<ThemeHelper::ThemeMode>(m_themeCombo->itemData(index).toInt());
        ThemeHelper::setThemeMode(mode);
    });

    layout->addWidget(new QLabel(QStringLiteral("Theme:"), page));
    layout->addWidget(m_themeCombo);
    layout->addStretch();

    addPage(page);
}

void SetupWizard::createBehaviourPage() {
    auto* page = new QWizardPage(this);
    page->setTitle(QStringLiteral("Behaviour"));
    page->setSubTitle(QStringLiteral("Configure how WinTools behaves."));

    auto* layout = new QVBoxLayout(page);

    m_closeToTray = new QCheckBox(QStringLiteral("Minimize to tray when closing"), page);
    m_closeToTray->setChecked(true);

    m_autoUpdate = new QCheckBox(QStringLiteral("Automatically check for updates on startup"), page);
    m_autoUpdate->setChecked(true);

    m_startWithWindows = new QCheckBox(QStringLiteral("Start WinTools with Windows"), page);
    m_startWithWindows->setChecked(false);

    layout->addWidget(m_closeToTray);
    layout->addSpacing(8);
    layout->addWidget(m_autoUpdate);
    layout->addSpacing(8);
    layout->addWidget(m_startWithWindows);
    layout->addStretch();

    addPage(page);
}

void SetupWizard::createFinishPage() {
    auto* page = new QWizardPage(this);
    page->setTitle(QStringLiteral("All Set!"));
    page->setSubTitle(QStringLiteral("Your preferences have been saved."));
    page->setFinalPage(true);

    auto* layout = new QVBoxLayout(page);
    auto* lbl = new QLabel(
        QStringLiteral("<p>WinTools is ready to use.</p>"
                       "<p>You can change these settings at any time from the "
                       "<b>Settings</b> page on the launch screen.</p>"), page);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    layout->addStretch();

    addPage(page);
}

void SetupWizard::applySelections() {
    QSettings s;

    if (m_themeCombo) {
        int mode = m_themeCombo->currentData().toInt();
        s.setValue(QStringLiteral("ui/global/theme_mode"), mode);
        ThemeHelper::setThemeMode(static_cast<ThemeHelper::ThemeMode>(mode));
    }

    if (m_closeToTray)
        s.setValue(QStringLiteral("ui/global/close_to_tray"), m_closeToTray->isChecked());

    if (m_autoUpdate)
        s.setValue(QStringLiteral("ui/global/auto_update_check"), m_autoUpdate->isChecked());

    if (m_startWithWindows)
        setStartWithWindows(m_startWithWindows->isChecked());
}

}
