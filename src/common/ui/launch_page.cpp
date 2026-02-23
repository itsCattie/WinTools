#include "common/ui/launch_page.hpp"

#include "common/hotkey/hotkey_config.hpp"
#include "common/hotkey/hotkey_engine.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/tray/tray_manager.hpp"
#include "common/ui/hotkey_bind_dialog.hpp"
#include "common/ui/launch_page_section_factory.hpp"
#include "common/ui/screen_relative_size.hpp"
#include "logger/logger.hpp"
#include "modules/module_provider.hpp"

#include <optional>
#include <QApplication>
#include <QCloseEvent>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMoveEvent>
#include <QPoint>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// WinTools: launch page manages UI behavior and presentation.

namespace wintools::ui {

namespace {
constexpr const char* LogSource = "LaunchPage";
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
      m_quickAccessCard(new QGroupBox("Quick access", this)),
      m_hotkeysCard(new QGroupBox("Hotkeys", this)),
      m_modulesCard(new QGroupBox("Modules", this)),
      m_quickGrid(new QGridLayout()),
      m_hotkeysTable(new QTableWidget(this)),
      m_modulesTable(new QTableWidget(this)) {
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
    auto* statusColumn = new QVBoxLayout();
    auto* cardsRow = new QHBoxLayout();
    auto* leftCards = new QVBoxLayout();

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

    leftCards->addWidget(m_quickAccessCard, 1);
    leftCards->addWidget(m_hotkeysCard, 1);

    cardsRow->addLayout(leftCards, 1);
    cardsRow->addWidget(m_modulesCard, 1);

    root->addLayout(top);
    root->addSpacing(18);
    root->addLayout(cardsRow, 1);

    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, &LaunchPage::onThemeChanged);
        connect(m_statusValueLabel, &QLabel::linkActivated,
            this, &LaunchPage::onUpdateStatusActivated);

    m_tray->updateModules(m_modules);
    m_tray->show();
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
    connect(m_hotkeysTable, &QTableWidget::customContextMenuRequested,
            this, &LaunchPage::onHotkeyTableContextMenu);

    rebuildSectionContent();
    applyTheme(m_palette);
    applyUpdateStatusText();
    QTimer::singleShot(0, this, &LaunchPage::checkForUpdates);

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Launch page initialized.",
                                  QString("Modules=%1").arg(m_modules.size()));
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

    setStyleSheet(wintools::themes::FluentStyle::generate(palette));

    m_statusLabel->setStyleSheet(QString("color:%1;").arg(palette.mutedForeground.name()));
    applyUpdateStatusText();
    rebuildSectionContent();
}

QHash<QString, QString> LaunchPage::buildHotkeyDisplayMap() const {
    QHash<QString, QString> map;
    for (const auto& b : m_hotkeyEngine->bindings()) {
        if (!b.enabled || b.action.module.isEmpty()) continue;
        const QString disp = wintools::hotkeys::HotkeyEngine::displayString(b);
        if (map.contains(b.action.module))
            map[b.action.module] += ", " + disp;
        else
            map[b.action.module] = disp;
    }
    return map;
}

void LaunchPage::rebuildSectionContent() {
    const QHash<QString, QString> hotkeyDisplay = buildHotkeyDisplayMap();
    LaunchPageSectionFactory::buildQuickAccess(m_quickAccessCard, m_quickGrid, m_modules, m_palette);
    LaunchPageSectionFactory::buildHotkeys(m_hotkeysTable, m_modules, hotkeyDisplay, m_palette);
    LaunchPageSectionFactory::buildModules(m_modulesTable, m_modules, m_palette, [this]() {
        LaunchPageSectionFactory::buildQuickAccess(m_quickAccessCard, m_quickGrid, m_modules, m_palette);
        LaunchPageSectionFactory::buildHotkeys(m_hotkeysTable, m_modules, buildHotkeyDisplayMap(), m_palette);
    }, this);
}

void LaunchPage::applyUpdateStatusText() {
    if (m_isUpToDate) {
        m_statusValueLabel->setText("Up to Date");
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
}

void LaunchPage::checkForUpdates() {
    m_releaseInfo = wintools::update::Update::checkForUpdates();
    if (!m_releaseInfo.success) {
        wintools::logger::Logger::log(
            LogSource, wintools::logger::Severity::Warning,
            "Update check did not complete.", m_releaseInfo.errorMessage);
        return;
    }

    m_isUpToDate = !m_releaseInfo.updateAvailable;
    m_updateVersion = m_releaseInfo.latestVersion;
    applyUpdateStatusText();
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

    const auto* nameItem = m_hotkeysTable->item(row, 0);
    if (!nameItem) return;
    const QString moduleName = nameItem->text();
    if (moduleName.isEmpty() || moduleName == "null") return;

    openHotkeyDialog(moduleName, "toggle",
                     HotkeyBindDialog::actionIdToLabel("toggle"));
}

void LaunchPage::onHotkeyTableContextMenu(const QPoint& pos) {
    const QModelIndex idx = m_hotkeysTable->indexAt(pos);
    if (!idx.isValid()) return;

    const auto* nameItem = m_hotkeysTable->item(idx.row(), 0);
    if (!nameItem) return;
    const QString moduleName = nameItem->text();
    if (moduleName.isEmpty() || moduleName == "null") return;

    const wintools::modules::ModuleEntry* modEntry = nullptr;
    for (const auto& m : m_modules) {
        if (m.name == moduleName) { modEntry = &m; break; }
    }

    QMenu menu(this);
    menu.setTitle(moduleName);

    auto* title = menu.addAction(QString("Hotkeys for: %1").arg(moduleName));
    title->setEnabled(false);
    menu.addSeparator();

    auto addActionItem = [&](const QString& id) {
        const QString label = HotkeyBindDialog::actionIdToLabel(id);
        QAction* act = menu.addAction(QString("Set: %1").arg(label));
        connect(act, &QAction::triggered, this,
                [this, moduleName, id, label]() {
                    openHotkeyDialog(moduleName, id, label);
                });
    };

    addActionItem("toggle");

    if (modEntry && !modEntry->actions.isEmpty()) {
        menu.addSeparator();
        for (auto it = modEntry->actions.cbegin();
             it != modEntry->actions.cend(); ++it) {
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

    event->ignore();
    hide();
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Main window hidden to tray.");
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

}
