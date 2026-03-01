#include "modules/module_provider.hpp"

#include "logger/ui/log_viewer_window.hpp"
#include "modules/mediabar/src/base/mediabar_controller.hpp"
#include "modules/disksentinel/src/ui/disk_sentinel_window.hpp"
#include "modules/AdvancedTaskManager/src/ui/task_manager_window.hpp"
#include "modules/AdvancedTaskManager/src/ui/profiler_overlay_controller.hpp"
#include "modules/GameVault/src/ui/gamevault_window.hpp"
#include "modules/StreamVault/src/ui/streamvault_window.hpp"
#include "modules/AudioMaster/src/ui/audiomaster_window.hpp"

namespace wintools::modules {

namespace {

void smartLaunchMediaBar(QWidget* ) {
    MediaBarController::instance()->toggle();
}

template<typename WindowT>
void showModuleWindow(QWidget* parent) {
    auto* win = new WindowT(parent);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->show();
    win->raise();
    win->activateWindow();
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

    ModuleEntry audiomaster;
    audiomaster.name     = "AudioMaster";
    audiomaster.iconPath = QStringLiteral(":/icons/modules/audiomaster.svg");
    audiomaster.enabled  = true;
    audiomaster.launch = [](QWidget* parent) {
        showModuleWindow<wintools::audiomaster::AudioMasterWindow>(parent);
    };
    audiomaster.actions["open"] = []() {
        showModuleWindow<wintools::audiomaster::AudioMasterWindow>(nullptr);
    };
    audiomaster.actions["rotate_output"] = []() {
        wintools::audiomaster::rotateLinkedOutputDevice();
    };
    audiomaster.actions["rotate_input"] = []() {
        wintools::audiomaster::rotateLinkedInputDevice();
    };
    modules.push_back(std::move(audiomaster));

    ModuleEntry disksentinel;
    disksentinel.name     = "DiskSentinel";
    disksentinel.iconPath = QStringLiteral(":/icons/modules/disksentinel.svg");
    disksentinel.enabled  = true;
    disksentinel.launch  = [](QWidget* parent) {
        showModuleWindow<disksentinel::DiskSentinelWindow>(parent);
    };
    disksentinel.actions["open"] = []() {
        showModuleWindow<disksentinel::DiskSentinelWindow>(nullptr);
    };
    modules.push_back(std::move(disksentinel));

    ModuleEntry atm;
    atm.name     = "AdvancedTaskManager";
    atm.iconPath = QStringLiteral(":/icons/modules/taskmanager.svg");
    atm.enabled  = true;
    atm.launch  = [](QWidget* parent) {
        showModuleWindow<wintools::taskmanager::TaskManagerWindow>(parent);
    };
    atm.actions["open"] = []() {
        showModuleWindow<wintools::taskmanager::TaskManagerWindow>(nullptr);
    };
    atm.actions["toggle_profiler"] = []() {
        wintools::taskmanager::ProfilerOverlayController::instance()->toggleOverlay();
    };
    modules.push_back(std::move(atm));

    ModuleEntry gamevault;
    gamevault.name     = "GameVault";
    gamevault.iconPath = QStringLiteral(":/icons/modules/gamevault.svg");
    gamevault.enabled  = true;
    gamevault.launch  = [](QWidget* parent) {
        showModuleWindow<wintools::gamevault::GameVaultWindow>(parent);
    };
    gamevault.actions["open"] = []() {
        showModuleWindow<wintools::gamevault::GameVaultWindow>(nullptr);
    };
    modules.push_back(std::move(gamevault));

    ModuleEntry streamvault;
    streamvault.name     = "StreamVault";
    streamvault.iconPath = QStringLiteral(":/icons/modules/streamvault.svg");
    streamvault.enabled  = true;
    streamvault.launch  = [](QWidget* parent) {
        showModuleWindow<wintools::streamvault::StreamVaultWindow>(parent);
    };
    streamvault.actions["open"] = []() {
        showModuleWindow<wintools::streamvault::StreamVaultWindow>(nullptr);
    };
    modules.push_back(std::move(streamvault));

    ModuleEntry logviewer;
    logviewer.name     = "LogViewer";
    logviewer.iconPath = QStringLiteral(":/icons/modules/logviewer.svg");
    logviewer.enabled  = true;
    logviewer.launch  = [](QWidget* parent) {
        showModuleWindow<wintools::logviewer::LogViewerWindow>(parent);
    };
    logviewer.actions["open"] = []() {
        showModuleWindow<wintools::logviewer::LogViewerWindow>(nullptr);
    };
    modules.push_back(std::move(logviewer));

    return modules;
}

}
