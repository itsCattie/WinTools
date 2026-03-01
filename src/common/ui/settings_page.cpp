#include "common/ui/settings_page.hpp"
#include "common/platform/autostart.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/hotkey/hotkey_config.hpp"
#include "common/hotkey/hotkey_engine.hpp"
#include "common/ui/hotkey_bind_dialog.hpp"
#include "wintools_version.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace wintools::ui {

using namespace wintools::themes;
using namespace wintools::hotkeys;
using wintools::platform::isStartWithWindows;
using wintools::platform::setStartWithWindows;

SettingsPage::SettingsPage(HotkeyEngine* engine, QWidget* parent)
    : QDialog(parent), m_hotkeyEngine(engine) {
    setWindowTitle(QStringLiteral("Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app/wintools.svg")));
    setMinimumSize(560, 480);
    resize(640, 540);

    auto* mainLayout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    auto* generalTab = new QWidget;
    auto* hotkeysTab = new QWidget;
    auto* aboutTab   = new QWidget;

    buildGeneralTab(generalTab);
    buildHotkeysTab(hotkeysTab);
    buildAboutTab(aboutTab);

    m_tabs->addTab(generalTab,  QStringLiteral("General"));
    m_tabs->addTab(hotkeysTab,  QStringLiteral("Hotkeys"));
    m_tabs->addTab(aboutTab,    QStringLiteral("About"));

    mainLayout->addWidget(m_tabs);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    mainLayout->addLayout(btnRow);

    m_themeListener = new ThemeListener(this);
    connect(m_themeListener, &ThemeListener::themeChanged,
            this, [this](bool) { applyTheme(ThemeHelper::currentPalette()); });

    loadSettings();
    applyTheme(ThemeHelper::currentPalette());
}

void SettingsPage::buildGeneralTab(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);

    auto* appearGroup = new QGroupBox(QStringLiteral("Appearance"), tab);
    auto* appearLay = new QFormLayout(appearGroup);
    m_themeCombo = new QComboBox(tab);
    ThemeHelper::populateThemeCombo(m_themeCombo);
    appearLay->addRow(QStringLiteral("Theme:"), m_themeCombo);
    layout->addWidget(appearGroup);

    auto* behaviourGroup = new QGroupBox(QStringLiteral("Behaviour"), tab);
    auto* behaviourLay = new QVBoxLayout(behaviourGroup);
    m_closeToTray = new QCheckBox(QStringLiteral("Minimize to tray when closing"), tab);
    m_autoUpdate  = new QCheckBox(QStringLiteral("Automatically check for updates on startup"), tab);
    m_startWithWindows = new QCheckBox(QStringLiteral("Start WinTools with Windows"), tab);
    behaviourLay->addWidget(m_closeToTray);
    behaviourLay->addWidget(m_autoUpdate);
    behaviourLay->addWidget(m_startWithWindows);
    layout->addWidget(behaviourGroup);

    layout->addStretch();

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onThemeChanged);
    connect(m_closeToTray, &QCheckBox::toggled,
            this, &SettingsPage::onCloseToTrayToggled);
    connect(m_autoUpdate, &QCheckBox::toggled,
            this, &SettingsPage::onAutoUpdateToggled);
    connect(m_startWithWindows, &QCheckBox::toggled,
            this, &SettingsPage::onStartWithWindowsToggled);
}

void SettingsPage::buildHotkeysTab(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);

    m_hotkeyTable = new QTableWidget(0, 4, tab);
    m_hotkeyTable->setHorizontalHeaderLabels({
        QStringLiteral("Action"),
        QStringLiteral("Module"),
        QStringLiteral("Key"),
        QStringLiteral("Enabled"),
    });
    m_hotkeyTable->horizontalHeader()->setStretchLastSection(false);
    m_hotkeyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_hotkeyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_hotkeyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_hotkeyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_hotkeyTable->verticalHeader()->setVisible(false);
    m_hotkeyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_hotkeyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_hotkeyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_hotkeyTable);

    auto* btnRow = new QHBoxLayout;
    auto* rebindBtn = new QPushButton(QStringLiteral("Rebind Selected..."), tab);
    connect(rebindBtn, &QPushButton::clicked, this, [this]() {
        int row = m_hotkeyTable->currentRow();
        if (row >= 0) rebindHotkey(row);
    });
    m_resetHotkeys = new QPushButton(QStringLiteral("Reset to Defaults"), tab);
    connect(m_resetHotkeys, &QPushButton::clicked, this, &SettingsPage::resetHotkeysToDefaults);
    btnRow->addWidget(rebindBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_resetHotkeys);
    layout->addLayout(btnRow);

    populateHotkeyTable();
}

void SettingsPage::buildAboutTab(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);
    layout->setAlignment(Qt::AlignCenter);

    auto* icon = new QLabel(tab);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/app/wintools.svg")).pixmap(96, 96));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto* title = new QLabel(QStringLiteral("<h2>WinTools v%1</h2>")
                                 .arg(QStringLiteral(WINTOOLS_APP_VERSION_LITERAL)), tab);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* desc = new QLabel(QStringLiteral(
        "<p>A modern Windows utility suite.</p>"
        "<p>Built with Qt %1 and C++20.</p>"
        "<p style='color:gray;font-size:11px;'>"
        "&copy; 2025 WinTools contributors. All rights reserved.</p>")
            .arg(QString::fromUtf8(qVersion())), tab);
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addStretch();
}

void SettingsPage::loadSettings() {
    QSettings s;

    int mode = s.value(QStringLiteral("ui/global/theme_mode"), 0).toInt();
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemData(i).toInt() == mode) {
            m_themeCombo->setCurrentIndex(i);
            break;
        }
    }

    m_closeToTray->setChecked(s.value(QStringLiteral("ui/global/close_to_tray"), true).toBool());

    m_autoUpdate->setChecked(s.value(QStringLiteral("ui/global/auto_update_check"), true).toBool());

    m_startWithWindows->setChecked(isStartWithWindows());
}

void SettingsPage::onThemeChanged(int index) {
    if (index < 0) return;
    int mode = m_themeCombo->itemData(index).toInt();
    auto themeMode = static_cast<ThemeHelper::ThemeMode>(mode);
    ThemeHelper::setThemeMode(themeMode);
    QSettings s;
    s.setValue(QStringLiteral("ui/global/theme_mode"), mode);
    emit settingsChanged();
}

void SettingsPage::onCloseToTrayToggled(bool checked) {
    QSettings s;
    s.setValue(QStringLiteral("ui/global/close_to_tray"), checked);
    emit settingsChanged();
}

void SettingsPage::onAutoUpdateToggled(bool checked) {
    QSettings s;
    s.setValue(QStringLiteral("ui/global/auto_update_check"), checked);
    emit settingsChanged();
}

void SettingsPage::onStartWithWindowsToggled(bool checked) {
    setStartWithWindows(checked);
    emit settingsChanged();
}

void SettingsPage::populateHotkeyTable() {
    auto bindings = m_hotkeyEngine ? m_hotkeyEngine->bindings() : HotkeyConfig::load();
    m_hotkeyTable->setRowCount(bindings.size());
    for (int i = 0; i < bindings.size(); ++i) {
        const auto& b = bindings[i];

        auto* actionItem = new QTableWidgetItem(b.action.id);
        auto* moduleItem = new QTableWidgetItem(b.action.module);

        QString keyStr = b.modifiers.join(QStringLiteral(" + "));
        if (!keyStr.isEmpty() && !b.keyString.isEmpty())
            keyStr += QStringLiteral(" + ");
        keyStr += b.keyString;
        auto* keyItem = new QTableWidgetItem(keyStr);

        auto* enabledItem = new QTableWidgetItem(b.enabled ? QStringLiteral("Yes")
                                                            : QStringLiteral("No"));
        enabledItem->setTextAlignment(Qt::AlignCenter);

        m_hotkeyTable->setItem(i, 0, actionItem);
        m_hotkeyTable->setItem(i, 1, moduleItem);
        m_hotkeyTable->setItem(i, 2, keyItem);
        m_hotkeyTable->setItem(i, 3, enabledItem);
    }
}

void SettingsPage::rebindHotkey(int row) {
    auto bindings = m_hotkeyEngine ? m_hotkeyEngine->bindings() : HotkeyConfig::load();
    if (row < 0 || row >= bindings.size()) return;

    const auto& b = bindings[row];
    auto palette = ThemeHelper::currentPalette();
    HotkeyBindDialog dlg(b.action.module, b.action.id,
                          HotkeyBindDialog::actionIdToLabel(b.action.id),
                          &b, palette, this);
    if (dlg.exec() == QDialog::Accepted) {
        auto captured = dlg.capturedBinding();
        if (captured.has_value()) {
            bindings[row] = captured.value();
        } else if (dlg.cleared()) {
            bindings[row].keyString.clear();
            bindings[row].modifiers.clear();
            bindings[row].enabled = false;
        }
        HotkeyConfig::save(bindings);
        if (m_hotkeyEngine) m_hotkeyEngine->applyBindings(bindings);
        populateHotkeyTable();
        emit settingsChanged();
    }
}

void SettingsPage::resetHotkeysToDefaults() {
    auto defaults = HotkeyConfig::defaults();
    HotkeyConfig::save(defaults);
    if (m_hotkeyEngine) m_hotkeyEngine->applyBindings(defaults);
    populateHotkeyTable();
    emit settingsChanged();
}

void SettingsPage::applyTheme(const ThemePalette& palette) {
    setStyleSheet(FluentStyle::generate(palette));
    if (m_hotkeyTable)
        m_hotkeyTable->setStyleSheet(FluentStyle::tableStyle(palette));
}

}
