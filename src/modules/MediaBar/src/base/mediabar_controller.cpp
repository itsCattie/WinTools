// MediaBar: mediabar controller manages feature behavior.

#include "modules/mediabar/src/base/mediabar_controller.hpp"

#include "modules/mediabar/src/app/app_main.hpp"
#include "logger/logger.hpp"

namespace wintools::modules {

namespace {
constexpr const char* LogSource = "MediaBarCtrl";
}

MediaBarController* MediaBarController::instance() {
    static MediaBarController inst;
    return &inst;
}

MediaBarController::MediaBarController(QObject* parent) : QObject(parent) {}

bool MediaBarController::isRunning() const {
    return m_app != nullptr;
}

void MediaBarController::ensureStarted() {
    if (m_app) return;
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Initialising MediaBar in-process.");
    m_app = new LyricsApp();
    m_app->initInProcess();
}

void MediaBarController::toggle() {
    ensureStarted();
    m_app->showMainWindow();
}

void MediaBarController::launchMini() {
    ensureStarted();
    m_app->showMiniPlayer();
}

void MediaBarController::launchFull() {
    ensureStarted();
    m_app->showMainWindow();
}

}
