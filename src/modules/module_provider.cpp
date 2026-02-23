#include "modules/module_provider.hpp"

#include "logger/ui/log_viewer_window.hpp"
#include "modules/mediabar/src/base/mediabar_controller.hpp"
#include "modules/disksentinel/src/ui/disk_sentinel_window.hpp"
#include "modules/AdvancedTaskManager/src/ui/task_manager_window.hpp"
#include "modules/GameVault/src/ui/gamevault_window.hpp"
#include "modules/StreamVault/src/ui/streamvault_window.hpp"

// module_provider.cpp: module provider manages feature behavior.

namespace wintools::modules {

namespace {

void smartLaunchMediaBar(QWidget* ) {
    MediaBarController::instance()->toggle();
}

}

std::vector<ModuleEntry> ModuleProvider::loadModules() {
    std::vector<ModuleEntry> modules;

    ModuleEntry mediabar;
    mediabar.name     = "MediaBar";
    mediabar.iconPath = QStringLiteral(":/icons/modules/mediabar.svg");
    mediabar.enabled  = true;

    mediabar.launch = [](QWidget* parent) { smartLaunchMediaBar(parent); };

    mediabar.actions["toggle"] = []() {
        MediaBarController::instance()->toggle();
    };
    mediabar.actions["show_mini"] = []() {
        MediaBarController::instance()->launchMini();
    };
    mediabar.actions["show_full"] = []() {
        MediaBarController::instance()->launchFull();
    };

    modules.push_back(std::move(mediabar));

    ModuleEntry disksentinel;
    disksentinel.name     = "DiskSentinel";
    disksentinel.iconPath = QStringLiteral(":/icons/modules/disksentinel.svg");
    disksentinel.enabled  = true;
    disksentinel.launch  = [](QWidget* parent) {
        auto* win = new disksentinel::DiskSentinelWindow(parent);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    disksentinel.actions["open"] = []() {
        auto* win = new disksentinel::DiskSentinelWindow(nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    modules.push_back(std::move(disksentinel));

    ModuleEntry atm;
    atm.name     = "AdvancedTaskManager";
    atm.iconPath = QStringLiteral(":/icons/modules/taskmanager.svg");
    atm.enabled  = true;
    atm.launch  = [](QWidget* parent) {
        auto* win = new wintools::taskmanager::TaskManagerWindow(parent);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    atm.actions["open"] = []() {
        auto* win = new wintools::taskmanager::TaskManagerWindow(nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    modules.push_back(std::move(atm));

    ModuleEntry gamevault;
    gamevault.name     = "GameVault";
    gamevault.iconPath = QStringLiteral(":/icons/modules/gamevault.svg");
    gamevault.enabled  = true;
    gamevault.launch  = [](QWidget* parent) {
        auto* win = new wintools::gamevault::GameVaultWindow(parent);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    gamevault.actions["open"] = []() {
        auto* win = new wintools::gamevault::GameVaultWindow(nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    modules.push_back(std::move(gamevault));

    ModuleEntry streamvault;
    streamvault.name     = "StreamVault";
    streamvault.iconPath = QStringLiteral(":/icons/modules/streamvault.svg");
    streamvault.enabled  = true;
    streamvault.launch  = [](QWidget* parent) {
        auto* win = new wintools::streamvault::StreamVaultWindow(parent);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    streamvault.actions["open"] = []() {
        auto* win = new wintools::streamvault::StreamVaultWindow(nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    modules.push_back(std::move(streamvault));

    ModuleEntry logviewer;
    logviewer.name     = "LogViewer";
    logviewer.iconPath = QStringLiteral(":/icons/modules/logviewer.svg");
    logviewer.enabled  = true;
    logviewer.launch  = [](QWidget* parent) {
        auto* win = new wintools::logviewer::LogViewerWindow(parent);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    logviewer.actions["open"] = []() {
        auto* win = new wintools::logviewer::LogViewerWindow(nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();
        win->raise();
        win->activateWindow();
    };
    modules.push_back(std::move(logviewer));

    return modules;
}

}
