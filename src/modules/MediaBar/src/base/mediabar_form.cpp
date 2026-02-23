#include "modules/mediabar/src/base/mediabar_form.hpp"

#include "logger/logger.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

// MediaBar: mediabar form manages feature behavior.

namespace wintools::modules {

namespace {
constexpr const char* LogSource = "MediaBarLauncher";
}

MediaBarForm::MediaBarForm(QWidget* parent)
    : QDialog(parent),
      m_statusLabel(new QLabel("Launching MediaBar...", this)),
      m_executableName("MediaBarCpp.exe") {
    setWindowTitle("MediaBar Launcher");
    setMinimumSize(460, 165);

    auto* layout = new QVBoxLayout(this);
    m_statusLabel->setWordWrap(true);

    auto* buttonRow = new QHBoxLayout();
    auto* launchButton = new QPushButton("Launch MediaBar", this);
    auto* buildButton = new QPushButton("Build MediaBar", this);

    connect(launchButton, &QPushButton::clicked, this, &MediaBarForm::tryLaunchAndCloseOnSuccess);
    connect(buildButton, &QPushButton::clicked, this, &MediaBarForm::runBuildScript);

    buttonRow->addWidget(launchButton);
    buttonRow->addWidget(buildButton);

    layout->addWidget(m_statusLabel);
    layout->addStretch();
    layout->addLayout(buttonRow);

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass, "Launcher initialized.");

    QTimer::singleShot(0, this, &MediaBarForm::tryLaunchAndCloseOnSuccess);
}

void MediaBarForm::tryLaunchAndCloseOnSuccess() {
    const QString executablePath = findMediaBarExecutable();
    if (executablePath.trimmed().isEmpty()) {
        m_statusLabel->setText("MediaBar executable not found. Build it with scripts\\build-mediabar.ps1, then click Launch again.");
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning, "Executable not found.");
        return;
    }

    if (QProcess::startDetached(executablePath, {}, QFileInfo(executablePath).absolutePath())) {
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass, "Launched MediaBar executable.", executablePath);
        close();
        return;
    }

    m_statusLabel->setText("Failed to launch MediaBar executable.");
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Error, "Failed to launch MediaBar executable.", executablePath);
}

void MediaBarForm::runBuildScript() {
    const QString repoRoot = findRepositoryRoot();
    const QString scriptPath = QDir(repoRoot).filePath("scripts/build-mediabar.ps1");
    if (!QFileInfo::exists(scriptPath)) {
        m_statusLabel->setText("Build script not found at scripts\\build-mediabar.ps1.");
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Error, "Build script not found.", scriptPath);
        return;
    }

    if (QProcess::startDetached("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", scriptPath}, repoRoot)) {
        m_statusLabel->setText("Build script started in a new PowerShell window.");
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass, "Build script started.", scriptPath);
        return;
    }

    m_statusLabel->setText("Failed to start build script.");
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Error, "Failed to start build script.", scriptPath);
}

QString MediaBarForm::findMediaBarExecutable() const {
    const QString repoRoot = findRepositoryRoot();
    const QString appBase = QCoreApplication::applicationDirPath();

    const QStringList candidates = {
        QDir(repoRoot).filePath("src/modules/MediaBar/build/Release/MediaBarCpp.exe"),
        QDir(repoRoot).filePath("src/modules/MediaBar/build/MediaBarCpp.exe"),
        QDir(appBase).filePath("modules/MediaBar/MediaBarCpp.exe"),
        QDir(appBase).filePath("MediaBarCpp.exe")
    };

    for (const QString& candidate : candidates) {
        if (isDeploymentReady(candidate)) {
            wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass, "Using deployment-ready executable.", candidate);
            return candidate;
        }
    }

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning, "No deployment-ready executable found.");
    return {};
}

QString MediaBarForm::findRepositoryRoot() const {
    QDir current(QCoreApplication::applicationDirPath());
    while (current.exists() && !current.isRoot()) {
        if (QFileInfo::exists(current.filePath("CMakeLists.txt")) &&
            QFileInfo::exists(current.filePath("scripts/build-mediabar.ps1"))) {
            return current.absolutePath();
        }
        if (!current.cdUp()) {
            break;
        }
    }

    return QCoreApplication::applicationDirPath();
}

bool MediaBarForm::isDeploymentReady(const QString& executablePath) {
    const QFileInfo exeInfo(executablePath);
    if (!exeInfo.exists() || !exeInfo.isFile()) {
        return false;
    }

    const QDir dir = exeInfo.dir();
    return QFileInfo::exists(dir.filePath("Qt6Core.dll")) && QFileInfo::exists(dir.filePath("platforms/qwindows.dll"));
}

}
