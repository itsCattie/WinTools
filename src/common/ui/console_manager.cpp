#include "common/ui/console_manager.hpp"

#include "logger/logger.hpp"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

#ifdef _WIN32
#include <windows.h>
#include <vector>
#endif

namespace wintools::ui {

namespace {
qint64 g_logViewerPid = 0;

bool isPidRunning(qint64 pid) {
    if (pid <= 0) {
        return false;
    }

#ifdef _WIN32
    QProcess checker;
    QStringList args;
    args << "/FI" << QString("PID eq %1").arg(pid);
    checker.start("tasklist", args);
    if (!checker.waitForFinished(3000)) {
        return false;
    }

    const QString output = QString::fromLocal8Bit(checker.readAllStandardOutput());
    if (output.contains("No tasks are running", Qt::CaseInsensitive)) {
        return false;
    }

    return output.contains(QString::number(pid));
#else

    return QFile::exists(QString("/proc/%1").arg(pid));
#endif
}

QString logScriptPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(base);
    dir.mkpath("logs");
    return dir.filePath("logs/wintools-log-viewer.ps1");
}

bool writeLogViewerScript(const QString& logPath) {
    const QString escapedLogPath = QString(logPath).replace("'", "''");
    const QString script =
        "$Host.UI.RawUI.WindowTitle='WinTools Logs';\n"
        "$ErrorActionPreference='Continue';\n"
        "Write-Host ('Streaming: ' + (Get-Date).ToString('u')) -ForegroundColor DarkGray;\n"
        "Get-Content -Path '" + escapedLogPath + "' -Tail 200 -Wait | ForEach-Object {\n"
        "  if ($_ -match '\\[(ERROR|Error)\\]') { Write-Host $_ -ForegroundColor Red }\n"
        "  elseif ($_ -match '\\[(WARN|Warning)\\]') { Write-Host $_ -ForegroundColor Yellow }\n"
        "  elseif ($_ -match '\\[(INFO|Pass)\\]') { Write-Host $_ -ForegroundColor Cyan }\n"
        "  elseif ($_ -match '\\[(TRACE)\\]') { Write-Host $_ -ForegroundColor DarkGray }\n"
        "  else { Write-Host $_ }\n"
        "}\n";

    QFile scriptFile(logScriptPath());
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&scriptFile);
    out << script;
    scriptFile.close();
    return true;
}

QString powershellPath() {
#ifdef _WIN32
    const QString resolved = QStandardPaths::findExecutable("powershell.exe");
    if (!resolved.isEmpty()) {
        return resolved;
    }

    return "C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe";
#elif defined(Q_OS_MACOS)
    const QString resolved = QStandardPaths::findExecutable("pwsh");
    return resolved.isEmpty() ? QStringLiteral("/usr/local/bin/pwsh") : resolved;
#else

    const QString resolved = QStandardPaths::findExecutable("pwsh");
    return resolved.isEmpty() ? QStringLiteral("bash") : resolved;
#endif
}

bool closeLogWindowByPid(qint64 pid) {
    if (pid <= 0) {
        return false;
    }

#ifdef _WIN32
    QStringList args;
    args << "/PID" << QString::number(pid) << "/T" << "/F";
    const int code = QProcess::execute("taskkill", args);
    return code == 0;
#else
    QStringList args;
    args << "-9" << QString::number(pid);
    const int code = QProcess::execute("kill", args);
    return code == 0;
#endif
}

bool openLogWindow(const QString& logPath, qint64* pid) {
    if (!writeLogViewerScript(logPath)) {
        return false;
    }

    const QString psPath = QDir::toNativeSeparators(powershellPath());
    const QString scriptPath = QDir::toNativeSeparators(logScriptPath());

#ifdef _WIN32
    const std::wstring app = psPath.toStdWString();
    const std::wstring cmd =
        L"\"" + app + L"\" -NoLogo -NoExit -ExecutionPolicy Bypass -File \"" + scriptPath.toStdWString() + L"\"";

    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};
    const BOOL started = CreateProcessW(
        app.c_str(),
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    if (!started) {
        return false;
    }

    if (pid) {
        *pid = static_cast<qint64>(processInfo.dwProcessId);
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
#else
    QStringList args;
    args << "-NoLogo"
         << "-NoExit"
         << "-ExecutionPolicy"
         << "Bypass"
         << "-File"
         << scriptPath;
    return QProcess::startDetached(psPath, args, QString(), pid);
#endif
}
}

bool ConsoleManager::isConsoleVisible() {
    if (!isPidRunning(g_logViewerPid)) {
        g_logViewerPid = 0;
        return false;
    }

    return true;
}

bool ConsoleManager::toggleConsole() {
    using wintools::logger::Logger;
    using wintools::logger::Severity;

    if (isConsoleVisible()) {
        const bool closed = closeLogWindowByPid(g_logViewerPid);
        g_logViewerPid = 0;

        if (closed) {
            Logger::log("ConsoleManager", Severity::Pass, "Closed log viewer process.");
        } else {
            Logger::log("ConsoleManager", Severity::Warning, "No active log viewer window was closed.");
        }

        return false;
    }

    const QString logPath = Logger::getLiveLogPath();
    qint64 launchedPid = 0;
    const bool started = openLogWindow(logPath, &launchedPid);
    if (!started) {
        Logger::log("ConsoleManager", Severity::Error, "Failed to open log viewer process.");
        return false;
    }

    g_logViewerPid = launchedPid;
    Logger::log("ConsoleManager", Severity::Pass, "Opened log viewer process.", QString("%1 | PID=%2").arg(logPath).arg(g_logViewerPid));
    return true;
}

}
