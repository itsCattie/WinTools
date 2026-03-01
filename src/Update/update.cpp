#include "update/update.hpp"

#include "logger/logger.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QVersionNumber>

namespace {
constexpr const char* LogSource = "Update";
constexpr const char* kReleaseApi = "https://api.github.com/repos/itsCattie/WinTools/releases/latest";
constexpr const char* kTagsApi = "https://api.github.com/repos/itsCattie/WinTools/tags?per_page=1";
constexpr const char* kReleasePage = "https://github.com/itsCattie/WinTools/releases/latest";

QString normalizedVersion(QString version) {
    version = version.trimmed();
    while (version.startsWith('v', Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }
    return version;
}

QVersionNumber parseComparableVersion(const QString& version) {
    const QString normalized = normalizedVersion(version);
    QVersionNumber direct = QVersionNumber::fromString(normalized);
    if (!direct.isNull()) {
        return direct;
    }

    static const QRegularExpression semverPrefixRe(
        QStringLiteral("^(\\d+(?:\\.\\d+)*)"));
    const QRegularExpressionMatch match = semverPrefixRe.match(normalized);
    if (match.hasMatch()) {
        return QVersionNumber::fromString(match.captured(1));
    }

    return {};
}

bool isNewerVersion(const QString& currentVersion, const QString& latestVersion) {
    const QString currentNormalized = normalizedVersion(currentVersion);
    const QString latestNormalized = normalizedVersion(latestVersion);

    if (latestNormalized.isEmpty()) {
        return false;
    }
    if (currentNormalized == latestNormalized) {
        return false;
    }

    const QVersionNumber currentComparable = parseComparableVersion(currentNormalized);
    const QVersionNumber latestComparable = parseComparableVersion(latestNormalized);
    if (!currentComparable.isNull() && !latestComparable.isNull()) {
        return QVersionNumber::compare(latestComparable, currentComparable) > 0;
    }

    return false;
}

bool isLikelyWindowsZip(const QString& fileName) {
    const QString lower = fileName.toLower();
    if (!lower.endsWith(".zip")) return false;
    return lower.contains("win") || lower.contains("windows") || lower.contains("portable") || lower.contains("native");
}

bool runBlockingRequest(const QNetworkRequest& request,
                        QByteArray* body,
                        QString* errorOut,
                        int* statusOut = nullptr) {

    QNetworkAccessManager manager;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(8000);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        if (statusOut) *statusOut = 0;
        if (errorOut) *errorOut = "Request timed out.";
        reply->deleteLater();
        return false;
    }

    const QByteArray responseBody = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusOut) *statusOut = statusCode;
    const QString reasonPhrase = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().trimmed();

    if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
        if (errorOut) {
            QString detail;
            QJsonParseError parseErr;
            const QJsonDocument errDoc = QJsonDocument::fromJson(responseBody, &parseErr);

            if (errDoc.isObject()) {
                const QJsonObject obj = errDoc.object();
                const QString apiMessage = obj.value("message").toString().trimmed();
                if (!apiMessage.isEmpty()) {
                    detail = apiMessage;
                }
            }

            if (detail.isEmpty() && !responseBody.isEmpty()) {
                detail = QString::fromUtf8(responseBody).trimmed().left(220);
            }
            if (detail.isEmpty()) {
                detail = reply->errorString().trimmed();
            }

            if (statusCode > 0) {
                if (!reasonPhrase.isEmpty()) {
                    *errorOut = QString("HTTP %1 %2: %3").arg(statusCode).arg(reasonPhrase, detail);
                } else {
                    *errorOut = QString("HTTP %1: %2").arg(statusCode).arg(detail);
                }
            } else {
                *errorOut = detail.isEmpty() ? QStringLiteral("Network request failed.") : detail;
            }
        }
        reply->deleteLater();
        return false;
    }

    if (body) {
        *body = responseBody;
    }
    reply->deleteLater();
    return true;
}

void applyGitHubApiHeaders(QNetworkRequest& req) {
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    const QString ua = QStringLiteral("WinTools/%1 (+https://github.com/itsCattie/WinTools)")
                           .arg(QCoreApplication::applicationVersion().trimmed().isEmpty()
                                   ? QStringLiteral("dev")
                                   : QCoreApplication::applicationVersion().trimmed());
    req.setHeader(QNetworkRequest::UserAgentHeader, ua);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
}

QString createUpdaterScript(const QString& scriptPath) {
    const QString script = QStringLiteral(
R"PS(param(
    [string]$SourceDir,
    [string]$TargetDir,
    [string]$ExeName,
    [int]$ParentPid
)

$ErrorActionPreference = 'Stop'

for ($i = 0; $i -lt 60; $i++) {
    if (-not (Get-Process -Id $ParentPid -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Milliseconds 500
}

if (-not (Test-Path -LiteralPath $SourceDir)) {
    throw "Update source folder not found: $SourceDir"
}

New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null

$copy = Start-Process -FilePath 'robocopy.exe' -ArgumentList @(
    $SourceDir,
    $TargetDir,
    '/E',
    '/R:2',
    '/W:1',
    '/NFL',
    '/NDL',
    '/NJH',
    '/NJS',
    '/NP'
) -PassThru -WindowStyle Hidden -Wait

if ($copy.ExitCode -ge 8) {
    throw "File copy failed with robocopy exit code $($copy.ExitCode)."
}

$exePath = Join-Path $TargetDir $ExeName
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Updated executable not found: $exePath"
}

Start-Process -FilePath $exePath -WorkingDirectory $TargetDir
)PS");

    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {};
    }
    f.write(script.toUtf8());
    f.close();
    return scriptPath;
}

QString detectPayloadRoot(const QString& extractedRoot, const QString& exeName) {
    QDir root(extractedRoot);
    const QString directExe = root.filePath(exeName);
    if (QFileInfo::exists(directExe)) {
        return extractedRoot;
    }

    const QFileInfoList children = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& child : children) {
        const QString candidate = QDir(child.absoluteFilePath()).filePath(exeName);
        if (QFileInfo::exists(candidate)) {
            return child.absoluteFilePath();
        }
    }

    return {};
}

}

namespace wintools::update {

ReleaseInfo Update::checkForUpdates() {
    ReleaseInfo out;
    out.currentVersion = QCoreApplication::applicationVersion().trimmed();
    out.releasePageUrl = QUrl(QString::fromLatin1(kReleasePage));

    QNetworkRequest req(QUrl(QString::fromLatin1(kReleaseApi)));
    applyGitHubApiHeaders(req);

    QByteArray body;
    QString err;
    int statusCode = 0;
    if (!runBlockingRequest(req, &body, &err, &statusCode)) {
        if (statusCode == 404) {

            wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning,
                                          "No published GitHub release found; falling back to latest tag.");

            QNetworkRequest tagsReq(QUrl(QString::fromLatin1(kTagsApi)));
            applyGitHubApiHeaders(tagsReq);

            QByteArray tagsBody;
            QString tagsErr;
            int tagsStatusCode = 0;
            if (!runBlockingRequest(tagsReq, &tagsBody, &tagsErr, &tagsStatusCode)) {
                out.errorMessage = tagsErr;
                wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning,
                                              "Update check failed.", tagsErr);
                return out;
            }

            QJsonParseError tagsParseError;
            const QJsonDocument tagsDoc = QJsonDocument::fromJson(tagsBody, &tagsParseError);
            if (!tagsDoc.isArray()) {
                out.errorMessage = tagsParseError.errorString().isEmpty()
                    ? QStringLiteral("Invalid tags metadata format.")
                    : tagsParseError.errorString();
                return out;
            }

            const QJsonArray tags = tagsDoc.array();
            if (tags.isEmpty()) {
                out.success = true;
                out.updateAvailable = false;
                out.errorMessage.clear();
                return out;
            }

            const QJsonObject latestTag = tags.first().toObject();
            out.latestVersion = latestTag.value("name").toString().trimmed();

            out.updateAvailable = isNewerVersion(out.currentVersion, out.latestVersion);

            out.success = true;
            out.errorMessage.clear();
            return out;
        }

        out.errorMessage = err;
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning,
                                      "Update check failed.", err);
        return out;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (!doc.isObject()) {
        out.errorMessage = parseError.errorString().isEmpty()
            ? QStringLiteral("Invalid release metadata format.")
            : parseError.errorString();
        return out;
    }

    const QJsonObject obj = doc.object();
    out.latestVersion = obj.value("tag_name").toString().trimmed();

    const QString htmlUrl = obj.value("html_url").toString().trimmed();
    if (!htmlUrl.isEmpty()) {
        out.releasePageUrl = QUrl(htmlUrl);
    }

    const QJsonArray assets = obj.value("assets").toArray();
    for (const QJsonValue& assetValue : assets) {
        const QJsonObject asset = assetValue.toObject();
        const QString name = asset.value("name").toString();
        const QString downloadUrl = asset.value("browser_download_url").toString();
        if (name.isEmpty() || downloadUrl.isEmpty()) continue;

        if (isLikelyWindowsZip(name)) {
            out.downloadFileName = name;
            out.downloadUrl = QUrl(downloadUrl);
            break;
        }
    }

    out.updateAvailable = isNewerVersion(out.currentVersion, out.latestVersion);

    out.success = true;
    return out;
}

bool Update::applyUpdate(const ReleaseInfo& release, QWidget* parent) {
    if (!release.updateAvailable) {
        return false;
    }

    if (!release.downloadUrl.isValid()) {
        if (release.releasePageUrl.isValid()) {
            QDesktopServices::openUrl(release.releasePageUrl);
            return false;
        }
        return false;
    }

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Cannot access temp folder for update."));
        return false;
    }

    const QString token = QString::fromLatin1(
        QCryptographicHash::hash(
            QString::number(QDateTime::currentMSecsSinceEpoch()).toUtf8(),
            QCryptographicHash::Sha1).toHex().left(8));
    const QString sessionDir = QDir(tempRoot).filePath(QStringLiteral("WinToolsUpdate_%1").arg(token));
    QDir().mkpath(sessionDir);

    const QString zipPath = QDir(sessionDir).filePath(
        release.downloadFileName.isEmpty() ? QStringLiteral("WinToolsUpdate.zip") : release.downloadFileName);

    QNetworkRequest dlReq(release.downloadUrl);
    dlReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
    dlReq.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("WinTools-Updater-Download"));

    QByteArray zipBytes;
    QString err;
    if (!runBlockingRequest(dlReq, &zipBytes, &err)) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Update download failed: %1").arg(err));
        return false;
    }

    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Cannot write update package to disk."));
        return false;
    }
    zipFile.write(zipBytes);
    zipFile.close();

    const QString extractedDir = QDir(sessionDir).filePath(QStringLiteral("payload"));
    QDir().mkpath(extractedDir);

    const QString psExtract = QStringLiteral(
        "-NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force\"")
        .arg(zipPath, extractedDir);
    const int extractExit = QProcess::execute(QStringLiteral("powershell.exe"),
                                              QProcess::splitCommand(psExtract));
    if (extractExit != 0) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Update package extraction failed."));
        return false;
    }

    const QString exePath = QCoreApplication::applicationFilePath();
    const QFileInfo exeInfo(exePath);
    const QString targetDir = exeInfo.absolutePath();
    const QString exeName = exeInfo.fileName();

    const QString payloadRoot = detectPayloadRoot(extractedDir, exeName);
    if (payloadRoot.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Could not locate updated app files in package."));
        return false;
    }

    QSettings syncSettings;
    syncSettings.sync();

    const QString scriptPath = QDir(sessionDir).filePath(QStringLiteral("apply-update.ps1"));
    if (createUpdaterScript(scriptPath).isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Could not prepare update installer script."));
        return false;
    }

    QStringList args;
    args << "-NoProfile"
         << "-ExecutionPolicy" << "Bypass"
         << "-File" << scriptPath
         << "-SourceDir" << payloadRoot
         << "-TargetDir" << targetDir
         << "-ExeName" << exeName
         << "-ParentPid" << QString::number(QCoreApplication::applicationPid());

    const bool started = QProcess::startDetached(QStringLiteral("powershell.exe"), args, targetDir);
    if (!started) {
        QMessageBox::warning(parent, QStringLiteral("Updater"),
                             QStringLiteral("Could not launch updater process."));
        return false;
    }

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Update installer launched.",
                                  QString("Version=%1").arg(release.latestVersion));
    return true;
}

}
