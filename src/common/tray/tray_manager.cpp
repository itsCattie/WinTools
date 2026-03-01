#include "common/tray/tray_manager.hpp"

#include "logger/logger.hpp"
#include "modules/module_provider.hpp"

#include <QApplication>
#include <QIcon>
#include <QMenu>

namespace wintools::ui {

namespace {
constexpr const char* LogSource = "TrayManager";
}

TrayManager::TrayManager(QObject* parent)
    : QObject(parent),
      m_tray(new QSystemTrayIcon(this)),
      m_menu(new QMenu()) {
    m_tray->setIcon(QIcon(QStringLiteral(":/icons/app/wintools.svg")));
    m_tray->setToolTip("WinTools");
    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated,
            this, &TrayManager::onTrayActivated);

    rebuildMenu();

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "System tray initialised.");
}

void TrayManager::show() {
    m_tray->show();
}

void TrayManager::setTooltip(const QString& text) {
    m_tray->setToolTip(text.isEmpty() ? QStringLiteral("WinTools") : text);
}

void TrayManager::showMessage(const QString& title, const QString& message,
                              QSystemTrayIcon::MessageIcon icon, int msecs) {
    m_tray->showMessage(title, message, icon, msecs);
}

void TrayManager::updateModules(
    const std::vector<wintools::modules::ModuleEntry>& modules) {
    rebuildMenu(&modules);
}

void TrayManager::rebuildMenu(
    const std::vector<wintools::modules::ModuleEntry>* modules) {
    m_menu->clear();

    QAction* openAction = m_menu->addAction("Open WinTools");
    connect(openAction, &QAction::triggered,
            this, &TrayManager::openMainWindowRequested);

    if (modules && !modules->empty()) {
        m_menu->addSeparator();
        for (const auto& mod : *modules) {
            if (!mod.enabled) continue;

            QMenu* sub = mod.iconPath.isEmpty()
                ? m_menu->addMenu(mod.name)
                : m_menu->addMenu(QIcon(mod.iconPath), mod.name);

            QAction* launch = sub->addAction("Open / Toggle");
            connect(launch, &QAction::triggered, this,
                    [this, name = mod.name]() {
                        emit moduleActionRequested(name, "toggle");
                    });

            if (!mod.actions.isEmpty()) {
                sub->addSeparator();
                for (auto it = mod.actions.cbegin();
                     it != mod.actions.cend(); ++it) {
                    QAction* act = sub->addAction(it.key());
                    connect(act, &QAction::triggered, this,
                            [this, name = mod.name, key = it.key()]() {
                                emit moduleActionRequested(name, key);
                            });
                }
            }
        }
    }

    m_menu->addSeparator();
    QAction* quitAction = m_menu->addAction("Quit WinTools");
    connect(quitAction, &QAction::triggered,
            this, &TrayManager::quitRequested);
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        emit openMainWindowRequested();
    }
}

}
