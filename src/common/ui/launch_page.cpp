#include "common/ui/launch_page.hpp"

#include "common/hotkey/hotkey_config.hpp"
#include "common/hotkey/hotkey_engine.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/tray/tray_manager.hpp"
#include "common/ui/hotkey_bind_dialog.hpp"
#include "common/ui/launch_page_section_factory.hpp"
#include "common/ui/screen_relative_size.hpp"
#include "common/ui/settings_page.hpp"
#include "common/ui/setup_wizard.hpp"
#include "modules/StreamVault/src/core/content_notification_service.hpp"
#include "logger/logger.hpp"
#include "modules/module_provider.hpp"
#include "wintools_version.hpp"

#include <optional>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCloseEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPoint>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wintools::ui {

namespace {
constexpr const char* LogSource = "LaunchPage";
constexpr const char* ModuleEnabledPrefix = "modules";
constexpr const char* HotkeysHeaderStateKey = "ui/hotkeys_table/header_state";
constexpr const char* HotkeysSortColumnKey = "ui/hotkeys_table/sort_column";
constexpr const char* HotkeysSortOrderKey = "ui/hotkeys_table/sort_order";
constexpr const char* HotkeysLayoutVersionKey = "ui/hotkeys_table/layout_version";
constexpr const char* CloseToTrayKey = "ui/global/close_to_tray";
constexpr const char* AutoUpdateCheckKey = "ui/global/auto_update_check";
constexpr int HotkeysLayoutVersion = 2;

bool readHotkeyRow(const QTableWidget* table,
                   int row,
                   QString& moduleName,
                   QString& actionId) {
    if (!table || row < 0 || row >= table->rowCount()) return false;

    const QTableWidgetItem* moduleItem = table->item(row, 0);
    const QTableWidgetItem* actionItem = table->item(row, 1);
    if (!moduleItem || !actionItem) return false;

    moduleName = moduleItem->data(Qt::UserRole).toString();
    actionId = actionItem->data(Qt::UserRole + 1).toString();
    if (moduleName.isEmpty() || actionId.isEmpty()) return false;
    return true;
}

QString moduleEnabledKey(const QString& moduleName) {
    return QString("%1/%2/enabled").arg(ModuleEnabledPrefix, moduleName);
}

void loadModuleEnabledState(std::vector<wintools::modules::ModuleEntry>& modules) {
    QSettings settings;
    for (auto& module : modules) {
        module.enabled = settings.value(moduleEnabledKey(module.name), module.enabled).toBool();
    }
}

void saveModuleEnabledState(const std::vector<wintools::modules::ModuleEntry>& modules) {
    QSettings settings;
    for (const auto& module : modules) {
        settings.setValue(moduleEnabledKey(module.name), module.enabled);
    }
    settings.sync();
}
}

LaunchPage::LaunchPage(QWidget* parent)
    : QMainWindow(parent),
      m_modules(wintools::modules::ModuleProvider::loadModules()),
      m_themeListener(new wintools::themes::ThemeListener(this)),
      m_monitors(this),
      m_tray(new wintools::ui::TrayManager(this)),
      m_hotkeyEngine(new wintools::hotkeys::HotkeyEngine(this)),
      m_palette(wintools::themes::ThemeHelper::currentPalette()),
      m_isUpToDate(true),
      m_titleLabel(new QLabel("Wintools", this)),
      m_statusLabel(new QLabel("Update Status", this)),
    m_statusValueLabel(new QLabel("Up to Date", this)),
        m_topBarFrame(new QFrame(this)),
      m_quickAccessCard(new QGroupBox("Quick access", this)),
      m_hotkeysCard(new QGroupBox("Hotkeys", this)),
      m_modulesCard(new QGroupBox("Modules", this)),
            m_settingsCard(new QGroupBox("Settings", this)),
      m_quickGrid(new QGridLayout()),
      m_hotkeysTable(new QTableWidget(this)),
            m_modulesTable(new QTableWidget(this)),

            m_themeModeCombo(new QComboBox(this)),
            m_closeToTrayCheck(new QCheckBox("Minimize to tray when closing main window", this)),
            m_autoUpdateCheck(new QCheckBox("Automatically check for updates on startup", this)) {
        loadModuleEnabledState(m_modules);

    setWindowTitle("WinTools");
    setMinimumSize(1000, 650);

    setWindowIcon(QIcon(QStringLiteral(":/icons/app/wintools.svg")));
    enableRelativeSizeAcrossScreens(this);

    m_titleLabel->setFont(QFont("Segoe UI", 26, QFont::Bold));
    m_statusLabel->setFont(QFont("Segoe UI", 11));
    m_statusValueLabel->setFont(QFont("Segoe UI", 18, QFont::Bold));

    m_statusValueLabel->setTextFormat(Qt::RichText);
    m_statusValueLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_statusValueLabel->setOpenExternalLinks(false);
    m_statusValueLabel->setCursor(Qt::ArrowCursor);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    auto* top = new QHBoxLayout();
    m_topBarFrame->setObjectName("launchTopBar");
    m_topBarFrame->setLayout(top);
    auto* statusColumn = new QVBoxLayout();
    auto* cardsRow = new QHBoxLayout();
    auto* leftCards = new QVBoxLayout();
    auto* rightCards = new QVBoxLayout();

    top->addWidget(m_titleLabel);
    top->addStretch();
    statusColumn->addWidget(m_statusValueLabel, 0, Qt::AlignRight);
    statusColumn->addWidget(m_statusLabel, 0, Qt::AlignRight);
    top->addLayout(statusColumn);

    m_quickAccessCard->setLayout(m_quickGrid);

    auto* hotkeyLayout = new QVBoxLayout();
    hotkeyLayout->addWidget(m_hotkeysTable);
    m_hotkeysCard->setLayout(hotkeyLayout);

    auto* modulesLayout = new QVBoxLayout();
    modulesLayout->addWidget(m_modulesTable);
    m_modulesCard->setLayout(modulesLayout);

    wintools::themes::ThemeHelper::populateThemeCombo(m_themeModeCombo);

    auto* settingsLayout = new QVBoxLayout();
    settingsLayout->addWidget(new QLabel("Theme", this));
    settingsLayout->addWidget(m_themeModeCombo);
    settingsLayout->addSpacing(8);
    settingsLayout->addWidget(m_closeToTrayCheck);
    settingsLayout->addWidget(m_autoUpdateCheck);
    settingsLayout->addSpacing(8);

    auto* openSettingsBtn = new QPushButton("Open Settings\u2026", this);
    openSettingsBtn->setCursor(Qt::PointingHandCursor);
    connect(openSettingsBtn, &QPushButton::clicked, this, [this]() {
        auto* dlg = new wintools::ui::SettingsPage(m_hotkeyEngine, this);
        connect(dlg, &wintools::ui::SettingsPage::settingsChanged, this, [this]() {
            loadGlobalSettings();
        });
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    settingsLayout->addWidget(openSettingsBtn);
    settingsLayout->addSpacing(4);

    auto* aboutBtn = new QPushButton("About WinTools", this);
    aboutBtn->setCursor(Qt::PointingHandCursor);
    connect(aboutBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox about(this);
        about.setWindowTitle("About WinTools");
        about.setIconPixmap(QIcon(":/icons/app/wintools.svg").pixmap(64, 64));
        about.setTextFormat(Qt::RichText);
        about.setText(
            QStringLiteral(
                "<h2>WinTools v%1</h2>"
                "<p>A modern Windows utility suite.</p>"
                "<p>Built with Qt %2 and C++20.</p>"
                "<p style='color:gray;font-size:11px;'>"
                "&copy; 2025 WinTools contributors. All rights reserved.</p>")
                .arg(WINTOOLS_APP_VERSION_LITERAL, qVersion()));
        about.exec();
    });
    settingsLayout->addWidget(aboutBtn);

    settingsLayout->addStretch();
    m_settingsCard->setLayout(settingsLayout);

    leftCards->addWidget(m_quickAccessCard, 1);
    leftCards->addWidget(m_hotkeysCard, 2);

    rightCards->addWidget(m_modulesCard, 2);
    rightCards->addWidget(m_settingsCard, 1);

    cardsRow->addLayout(leftCards, 1);
    cardsRow->addLayout(rightCards, 1);

    root->addWidget(m_topBarFrame);
    root->addSpacing(18);
    root->addLayout(cardsRow, 1);

    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, &LaunchPage::onThemeChanged);
        connect(m_statusValueLabel, &QLabel::linkActivated,
            this, &LaunchPage::onUpdateStatusActivated);

    m_tray->updateModules(m_modules);
    m_tray->show();
    updateTrayTooltip();
    connect(m_tray, &wintools::ui::TrayManager::openMainWindowRequested,
            this, &LaunchPage::showMainWindow);
    connect(m_tray, &wintools::ui::TrayManager::moduleActionRequested,
            this, &LaunchPage::dispatchModuleAction);
    connect(m_tray, &wintools::ui::TrayManager::quitRequested,
            qApp, &QApplication::quit);

    m_hotkeyEngine->applyBindings(wintools::hotkeys::HotkeyConfig::load());
    connect(m_hotkeyEngine, &wintools::hotkeys::HotkeyEngine::hotkeyTriggered,
            this, &LaunchPage::onHotkeyTriggered);

    m_hotkeysTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_hotkeysTable, &QTableWidget::cellClicked,
            this, &LaunchPage::onHotkeyTableCellClicked);
        connect(m_hotkeysTable, &QTableWidget::cellActivated,
            this, &LaunchPage::onHotkeyTableCellClicked);
    connect(m_hotkeysTable, &QTableWidget::customContextMenuRequested,
            this, &LaunchPage::onHotkeyTableContextMenu);

        connect(m_hotkeysTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int, int, int) { saveHotkeysTablePreferences(); });
        connect(m_hotkeysTable->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, [this](int, Qt::SortOrder) { saveHotkeysTablePreferences(); });

        connect(m_themeModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            const auto mode = static_cast<wintools::themes::ThemeHelper::ThemeMode>(
                m_themeModeCombo->currentData().toInt());
            wintools::themes::ThemeHelper::setThemeMode(mode);
            saveGlobalSettings();
            applyTheme(wintools::themes::ThemeHelper::currentPalette());
        });
        connect(m_closeToTrayCheck, &QCheckBox::toggled, this, [this](bool) {
            saveGlobalSettings();
        });
        connect(m_autoUpdateCheck, &QCheckBox::toggled, this, [this](bool) {
            saveGlobalSettings();
        });

        loadGlobalSettings();

    rebuildSectionContent();
    applyTheme(m_palette);
    applyUpdateStatusText();
        if (m_autoUpdateCheck->isChecked()) {
            QTimer::singleShot(0, this, &LaunchPage::checkForUpdates);
        }

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Launch page initialized.",
                                  QString("Modules=%1").arg(m_modules.size()));

    m_notifService = new wintools::streamvault::ContentNotificationService(m_tray, this);
    m_notifService->start(60);

    if (!wintools::ui::SetupWizard::hasCompletedSetup()) {
        auto* wizard = new wintools::ui::SetupWizard(m_hotkeyEngine, this);
        wizard->setAttribute(Qt::WA_DeleteOnClose);
        connect(wizard, &QWizard::accepted, this, [this]() { loadGlobalSettings(); });
        wizard->show();
    }
}

void LaunchPage::moveEvent(QMoveEvent* event) {
    QMainWindow::moveEvent(event);
    if (m_monitors.track(this)) {
        rebuildSectionContent();
    }
}

void LaunchPage::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (m_monitors.track(this)) {
        rebuildSectionContent();
    }
}

void LaunchPage::onThemeChanged(bool) {
    applyTheme(wintools::themes::ThemeHelper::currentPalette());
}

void LaunchPage::applyTheme(const wintools::themes::ThemePalette& palette) {
    m_palette = palette;

    wintools::themes::ThemeHelper::applyThemeTo(this);

    m_statusLabel->setStyleSheet(QString("color:%1;").arg(palette.mutedForeground.name()));
    m_titleLabel->setStyleSheet(QString("color:%1;").arg(palette.foreground.name()));
    m_topBarFrame->setStyleSheet(QString(
        "QFrame#launchTopBar {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "}").arg(palette.cardBackground.name(), palette.cardBorder.name()));

    applyUpdateStatusText();
    rebuildSectionContent();
}

QHash<QString, QString> LaunchPage::buildHotkeyDisplayMap() const {
    QHash<QString, QString> map;
    for (const auto& b : m_hotkeyEngine->bindings()) {
        if (!b.enabled || b.action.module.isEmpty()) continue;
        const QString disp = wintools::hotkeys::HotkeyEngine::displayString(b);
        const QString key = QString("%1.%2").arg(b.action.module, b.action.action);
        map[key] = disp;
    }
    return map;
}

void LaunchPage::rebuildSectionContent() {
    m_hotkeysTable->setSortingEnabled(false);

    const QHash<QString, QString> hotkeyDisplay = buildHotkeyDisplayMap();
    LaunchPageSectionFactory::buildQuickAccess(m_quickAccessCard, m_quickGrid, m_modules, m_palette);
    LaunchPageSectionFactory::buildHotkeys(
        m_hotkeysTable,
        m_modules,
        hotkeyDisplay,
        m_palette,
        [this](const QString& module, const QString& actionId) {
            openHotkeyDialog(module, actionId, HotkeyBindDialog::actionIdToLabel(actionId));
        });
    restoreHotkeysTablePreferences();

    LaunchPageSectionFactory::buildModules(m_modulesTable, m_modules, m_palette, [this]() {
        LaunchPageSectionFactory::buildQuickAccess(m_quickAccessCard, m_quickGrid, m_modules, m_palette);
        m_hotkeysTable->setSortingEnabled(false);
        LaunchPageSectionFactory::buildHotkeys(
            m_hotkeysTable,
            m_modules,
            buildHotkeyDisplayMap(),
            m_palette,
            [this](const QString& module, const QString& actionId) {
                openHotkeyDialog(module, actionId, HotkeyBindDialog::actionIdToLabel(actionId));
            });
        restoreHotkeysTablePreferences();
        saveModuleEnabledState(m_modules);
        m_tray->updateModules(m_modules);
        updateTrayTooltip();
    }, this);
}

void LaunchPage::restoreHotkeysTablePreferences() {
    if (!m_hotkeysTable || !m_hotkeysTable->horizontalHeader()) return;

    QSettings settings;
    QHeaderView* header = m_hotkeysTable->horizontalHeader();

    const int layoutVersion = settings.value(HotkeysLayoutVersionKey, 0).toInt();
    if (layoutVersion != HotkeysLayoutVersion) {
        settings.setValue(HotkeysLayoutVersionKey, HotkeysLayoutVersion);
        settings.remove(HotkeysHeaderStateKey);
        settings.setValue(HotkeysSortColumnKey, 0);
        settings.setValue(HotkeysSortOrderKey, static_cast<int>(Qt::AscendingOrder));
    }

    const QByteArray headerState = settings.value(HotkeysHeaderStateKey).toByteArray();
    if (!headerState.isEmpty()) {
        header->restoreState(headerState);
    }

    int sortColumn = settings.value(HotkeysSortColumnKey, 0).toInt();
    const Qt::SortOrder sortOrder =
        static_cast<Qt::SortOrder>(settings.value(HotkeysSortOrderKey,
                                                  static_cast<int>(Qt::AscendingOrder)).toInt());

    if (sortColumn < 0 || sortColumn >= m_hotkeysTable->columnCount() || sortColumn > 2) {
        sortColumn = 0;
    }

    m_hotkeysTable->setSortingEnabled(true);
    if (m_hotkeysTable->rowCount() > 0) {
        m_hotkeysTable->sortItems(sortColumn, sortOrder);
    }
    header->setSortIndicator(sortColumn, sortOrder);
}

void LaunchPage::saveHotkeysTablePreferences() const {
    if (!m_hotkeysTable || !m_hotkeysTable->horizontalHeader()) return;

    QHeaderView* header = m_hotkeysTable->horizontalHeader();
    int sortColumn = header->sortIndicatorSection();
    if (sortColumn < 0 || sortColumn > 2) {
        sortColumn = 0;
    }

    QSettings settings;
    settings.setValue(HotkeysLayoutVersionKey, HotkeysLayoutVersion);
    settings.setValue(HotkeysHeaderStateKey, header->saveState());
    settings.setValue(HotkeysSortColumnKey, sortColumn);
    settings.setValue(HotkeysSortOrderKey, static_cast<int>(header->sortIndicatorOrder()));
}

void LaunchPage::loadGlobalSettings() {
    QSettings settings;

    const int rawThemeMode = settings
        .value("ui/global/theme_mode",
               static_cast<int>(wintools::themes::ThemeHelper::ThemeMode::System))
        .toInt();

    int comboIndex = m_themeModeCombo->findData(rawThemeMode);
    if (comboIndex < 0) {
        comboIndex = m_themeModeCombo->findData(
            static_cast<int>(wintools::themes::ThemeHelper::ThemeMode::System));
    }

    {
        QSignalBlocker blocker(m_themeModeCombo);
        m_themeModeCombo->setCurrentIndex(comboIndex);
    }

    wintools::themes::ThemeHelper::setThemeMode(
        static_cast<wintools::themes::ThemeHelper::ThemeMode>(
            m_themeModeCombo->currentData().toInt()));

    {
        QSignalBlocker blocker(m_closeToTrayCheck);
        m_closeToTrayCheck->setChecked(settings.value(CloseToTrayKey, true).toBool());
    }
    {
        QSignalBlocker blocker(m_autoUpdateCheck);
        m_autoUpdateCheck->setChecked(settings.value(AutoUpdateCheckKey, true).toBool());
    }

    m_palette = wintools::themes::ThemeHelper::currentPalette();
}

void LaunchPage::saveGlobalSettings() const {
    QSettings settings;
    settings.setValue("ui/global/theme_mode", m_themeModeCombo->currentData().toInt());
    settings.setValue(CloseToTrayKey, m_closeToTrayCheck->isChecked());
    settings.setValue(AutoUpdateCheckKey, m_autoUpdateCheck->isChecked());
    settings.sync();
}

void LaunchPage::applyUpdateStatusText() {
    if (m_isUpToDate) {
        const QString ver = QCoreApplication::applicationVersion().trimmed();
        if (!ver.isEmpty()) {

            const QString verLabel = ver.startsWith('v', Qt::CaseInsensitive) ? ver : QString("v%1").arg(ver);
            m_statusValueLabel->setText(QString("Up to Date"));
            m_statusLabel->setText(verLabel);
        } else {
            m_statusValueLabel->setText("Up to Date");
            m_statusLabel->setText(QString());
        }
        m_statusValueLabel->setCursor(Qt::ArrowCursor);
        m_statusValueLabel->setToolTip(QString());
        m_statusValueLabel->setStyleSheet(QString("color:%1;").arg(m_palette.statusNeutral.name()));
        return;
    }

    const QString updateText = m_updateVersion.trimmed().isEmpty()
        ? QStringLiteral("Update Available")
        : QString("Update Available (%1)").arg(m_updateVersion);
    m_statusValueLabel->setText(QString("<a href=\"update\" style=\"text-decoration:none;\">%1</a>").arg(updateText.toHtmlEscaped()));
    m_statusValueLabel->setCursor(Qt::PointingHandCursor);
    m_statusValueLabel->setToolTip("Click to update from GitHub release");
    m_statusValueLabel->setStyleSheet(QString("color:%1;").arg(m_palette.statusUpdate.name()));

    if (!m_updateVersion.trimmed().isEmpty()) {
        const QString verLabel = m_updateVersion.trimmed().startsWith('v', Qt::CaseInsensitive)
            ? m_updateVersion.trimmed()
            : QString("v%1").arg(m_updateVersion.trimmed());
        m_statusLabel->setText(verLabel);
    } else {
        m_statusLabel->setText(QString());
    }
}

void LaunchPage::checkForUpdates() {
    m_silentUpdateCheck = true;

    auto* thread = new QThread(this);
    auto* ctx    = new QObject();
    ctx->moveToThread(thread);

    connect(thread, &QThread::started, ctx, [this, ctx, thread]() {
        auto info = wintools::update::Update::checkForUpdates();

        QMetaObject::invokeMethod(this,
            [this, info = std::move(info)]() { onUpdateCheckFinished(info); },
            Qt::QueuedConnection);
        ctx->deleteLater();
        thread->quit();
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void LaunchPage::onUpdateCheckFinished(wintools::update::ReleaseInfo info) {
    m_releaseInfo = std::move(info);

    if (!m_releaseInfo.success) {
        wintools::logger::Logger::log(
            LogSource, wintools::logger::Severity::Warning,
            "Update check did not complete.", m_releaseInfo.errorMessage);
        m_silentUpdateCheck = false;
        return;
    }

    m_isUpToDate = !m_releaseInfo.updateAvailable;
    m_updateVersion = m_releaseInfo.latestVersion;
    applyUpdateStatusText();

    if (m_silentUpdateCheck && m_releaseInfo.updateAvailable && m_tray) {
        const QString verText = m_releaseInfo.latestVersion.trimmed().isEmpty()
            ? QStringLiteral("A new version is available.")
            : QStringLiteral("Version %1 is available.").arg(m_releaseInfo.latestVersion);
        m_tray->showMessage(QStringLiteral("WinTools Update"), verText);
    }

    m_silentUpdateCheck = false;
}

void LaunchPage::onUpdateStatusActivated(const QString& link) {
    if (link != "update") return;
    if (m_isUpToDate) return;

    const bool installerStarted = wintools::update::Update::applyUpdate(m_releaseInfo, this);
    if (installerStarted) {
        qApp->quit();
    }
}

void LaunchPage::onHotkeyTableCellClicked(int row, int ) {
    QString moduleName;
    QString actionId;
    if (!readHotkeyRow(m_hotkeysTable, row, moduleName, actionId)) return;

    openHotkeyDialog(moduleName,
                     actionId,
                     HotkeyBindDialog::actionIdToLabel(actionId));
}

void LaunchPage::onHotkeyTableContextMenu(const QPoint& pos) {
    const QModelIndex idx = m_hotkeysTable->indexAt(pos);
    if (!idx.isValid()) return;

    QString moduleName;
    QString actionId;
    if (!readHotkeyRow(m_hotkeysTable, idx.row(), moduleName, actionId)) return;

    const wintools::modules::ModuleEntry* modEntry = nullptr;
    for (const auto& m : m_modules) {
        if (m.name == moduleName) { modEntry = &m; break; }
    }

    QMenu menu(this);
    menu.setTitle(moduleName);

    auto* title = menu.addAction(
        QString("%1 · %2")
            .arg(moduleName, HotkeyBindDialog::actionIdToLabel(actionId)));
    title->setEnabled(false);
    menu.addSeparator();

    QAction* setCurrent = menu.addAction("Set this hotkey");
    connect(setCurrent, &QAction::triggered, this,
            [this, moduleName, actionId]() {
                openHotkeyDialog(moduleName,
                                 actionId,
                                 HotkeyBindDialog::actionIdToLabel(actionId));
            });

    QAction* clearCurrent = menu.addAction("Clear this hotkey");
    connect(clearCurrent, &QAction::triggered, this, [this, moduleName, actionId]() {
        auto bindings = m_hotkeyEngine->bindings();
        for (auto& b : bindings) {
            if (b.action.module == moduleName && b.action.action == actionId) {
                b.enabled = false;
            }
        }
        m_hotkeyEngine->applyBindings(bindings);
        wintools::hotkeys::HotkeyConfig::save(bindings);
        rebuildSectionContent();
    });

    if (modEntry && !modEntry->actions.isEmpty()) {
        QMenu* otherActionsMenu = menu.addMenu("Set another action...");

        auto addActionItem = [&](const QString& id) {
            const QString label = HotkeyBindDialog::actionIdToLabel(id);
            QAction* act = otherActionsMenu->addAction(label);
            connect(act, &QAction::triggered, this,
                    [this, moduleName, id, label]() {
                        openHotkeyDialog(moduleName, id, label);
                    });
        };

        addActionItem("toggle");
        for (auto it = modEntry->actions.cbegin(); it != modEntry->actions.cend(); ++it) {
            if (it.key() == "toggle") continue;
            addActionItem(it.key());
        }
    }

    menu.addSeparator();
    QAction* clearAll = menu.addAction(QString("Clear all hotkeys for %1").arg(moduleName));
    connect(clearAll, &QAction::triggered, this, [this, moduleName]() {
        auto bindings = m_hotkeyEngine->bindings();
        for (auto& b : bindings) {
            if (b.action.module == moduleName) b.enabled = false;
        }
        m_hotkeyEngine->applyBindings(bindings);
        wintools::hotkeys::HotkeyConfig::save(bindings);
        rebuildSectionContent();
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                      QString("Cleared all hotkeys for %1.").arg(moduleName));
    });

    menu.exec(m_hotkeysTable->viewport()->mapToGlobal(pos));
}

void LaunchPage::openHotkeyDialog(const QString& module,
                                   const QString& actionId,
                                   const QString& actionLabel) {

    std::optional<wintools::hotkeys::HotkeyBinding> existingCopy;
    for (const auto& b : m_hotkeyEngine->bindings()) {
        if (b.action.module == module && b.action.action == actionId) {
            existingCopy = b;
            break;
        }
    }
    const wintools::hotkeys::HotkeyBinding* existing =
        existingCopy.has_value() ? &existingCopy.value() : nullptr;

    m_hotkeyEngine->applyBindings({});

    HotkeyBindDialog dlg(module, actionId, actionLabel, existing, m_palette, this);
    const int dialogResult = dlg.exec();

    m_hotkeyEngine->applyBindings(wintools::hotkeys::HotkeyConfig::load());

    if (dialogResult != QDialog::Accepted) return;

    auto bindings = m_hotkeyEngine->bindings();

    if (dlg.cleared()) {

        for (auto& b : bindings) {
            if (b.action.module == module && b.action.action == actionId)
                b.enabled = false;
        }
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                      QString("Cleared hotkey: %1.%2").arg(module, actionId));
    } else if (const auto newBinding = dlg.capturedBinding()) {
        bool found = false;
        for (auto& b : bindings) {
            if (b.action.module == module && b.action.action == actionId) {
                b = *newBinding;
                b.enabled = true;
                found = true;
                break;
            }
        }
        if (!found) bindings.append(*newBinding);

        wintools::logger::Logger::log(
            LogSource, wintools::logger::Severity::Pass,
            QString("Hotkey set: %1.%2 → %3")
                .arg(module, actionId,
                     wintools::hotkeys::HotkeyEngine::displayString(*newBinding)));
    }

    m_hotkeyEngine->applyBindings(bindings);
    wintools::hotkeys::HotkeyConfig::save(bindings);
    rebuildSectionContent();
}

void LaunchPage::closeEvent(QCloseEvent* event) {
    saveHotkeysTablePreferences();
    saveGlobalSettings();

    if (m_closeToTrayCheck->isChecked()) {
        event->ignore();
        hide();
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                      "Main window hidden to tray.");
        return;
    }

    event->accept();
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Main window closed.");
}

void LaunchPage::showMainWindow() {
    show();
    raise();
    activateWindow();
}

void LaunchPage::onHotkeyTriggered(
    const wintools::hotkeys::HotkeyAction& action) {
    wintools::logger::Logger::log(
        LogSource, wintools::logger::Severity::Pass,
        QString("Hotkey fired: %1.%2").arg(action.module, action.action));

    if (action.action == "open_main" || action.module.isEmpty()) {
        showMainWindow();
        return;
    }
    dispatchModuleAction(action.module, action.action);
}

void LaunchPage::dispatchModuleAction(const QString& module,
                                      const QString& action) {
    for (auto& mod : m_modules) {
        if (mod.name != module) continue;

        if (!mod.enabled) {
            wintools::logger::Logger::log(
                LogSource, wintools::logger::Severity::Warning,
                QString("dispatchModuleAction: module '%1' is disabled.").arg(module));
            return;
        }

        if (mod.actions.contains(action)) {
            if (auto fn = mod.actions.value(action)) {
                fn();
                return;
            }
        }

        if (mod.launch) {
            mod.launch(this);
        }
        return;
    }

    wintools::logger::Logger::log(
        LogSource, wintools::logger::Severity::Warning,
        QString("dispatchModuleAction: module '%1' not found.").arg(module));
}

void LaunchPage::updateTrayTooltip() {
    int enabledCount = 0;
    QStringList names;
    for (const auto& m : m_modules) {
        if (m.enabled) {
            ++enabledCount;
            names.append(m.name);
        }
    }

    QString tip = QStringLiteral("WinTools — %1 module%2")
        .arg(enabledCount)
        .arg(enabledCount == 1 ? "" : "s");

    if (!names.isEmpty()) {
        tip += QStringLiteral("\n") + names.join(QStringLiteral(", "));
    }

    m_tray->setTooltip(tip);
}

}
