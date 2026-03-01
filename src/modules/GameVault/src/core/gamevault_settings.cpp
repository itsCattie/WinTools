#include "gamevault_settings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace wintools::gamevault {

static constexpr char kOrg[]     = "WinTools";
static constexpr char kApp[]     = "GameVault";
static constexpr char kFolders[] = "customGameFolders";
static constexpr char kEmuGroup[]= "emulatorPaths";
static constexpr char kArtGroup[]= "customArtPaths";
static constexpr char kManualGames[] = "manualGames";
static constexpr char kGameOverrides[] = "gameExecutableOverrides";

namespace {

QJsonObject serializeManualGame(const GameEntry& game) {
    QJsonObject o;
    o.insert("title", game.title);
    o.insert("platform", static_cast<int>(game.platform));
    o.insert("systemTag", game.systemTag);
    o.insert("platformId", game.platformId);
    o.insert("installPath", game.installPath);
    o.insert("executablePath", game.executablePath);
    o.insert("launchUri", game.launchUri);
    o.insert("artBannerUrl", game.artBannerUrl);
    o.insert("artCapsuleUrl", game.artCapsuleUrl);
    o.insert("installed", game.installed);
    return o;
}

GameEntry deserializeManualGame(const QJsonObject& o) {
    GameEntry game;
    game.title = o.value("title").toString();
    game.platform = static_cast<GamePlatform>(o.value("platform").toInt(static_cast<int>(GamePlatform::Unknown)));
    game.systemTag = o.value("systemTag").toString();
    game.platformId = o.value("platformId").toString();
    game.installPath = o.value("installPath").toString();
    game.executablePath = o.value("executablePath").toString();
    game.launchUri = o.value("launchUri").toString();
    game.artBannerUrl = o.value("artBannerUrl").toString();
    game.artCapsuleUrl = o.value("artCapsuleUrl").toString();
    game.installed = o.value("installed").toBool(!game.executablePath.isEmpty());
    return game;
}

QString manualGameKey(const GameEntry& game) {
    const QString id = game.platformId.trimmed();
    if (!id.isEmpty()) return id.toLower();
    return game.title.trimmed().toLower() + "|" + game.executablePath.trimmed().toLower();
}

}

GameVaultSettings& GameVaultSettings::instance() {
    static GameVaultSettings s;
    return s;
}

QStringList GameVaultSettings::customGameFolders() const {
    QSettings s(kOrg, kApp);
    return s.value(kFolders).toStringList();
}

void GameVaultSettings::setCustomGameFolders(const QStringList& folders) {
    QSettings s(kOrg, kApp);
    s.setValue(kFolders, folders);
}

void GameVaultSettings::addCustomGameFolder(const QString& path) {
    auto folders = customGameFolders();
    if (path.isEmpty() || folders.contains(path)) return;
    folders << path;
    setCustomGameFolders(folders);
}

void GameVaultSettings::removeCustomGameFolder(const QString& path) {
    auto folders = customGameFolders();
    folders.removeAll(path);
    setCustomGameFolders(folders);
}

QString GameVaultSettings::emulatorPath(const QString& name) const {
    QSettings s(kOrg, kApp);
    s.beginGroup(kEmuGroup);
    return s.value(name).toString();
}

void GameVaultSettings::setEmulatorPath(const QString& name, const QString& path) {
    QSettings s(kOrg, kApp);
    s.beginGroup(kEmuGroup);
    s.setValue(name, path);
}

void GameVaultSettings::clearEmulatorPath(const QString& name) {
    QSettings s(kOrg, kApp);
    s.beginGroup(kEmuGroup);
    s.remove(name);
}

QStringList GameVaultSettings::emulatorOverrideNames() const {
    QSettings s(kOrg, kApp);
    s.beginGroup(kEmuGroup);
    return s.childKeys();
}

QString GameVaultSettings::customArtPath(const QString& entryKey) const {
    if (entryKey.trimmed().isEmpty()) return {};
    QSettings s(kOrg, kApp);
    s.beginGroup(kArtGroup);
    return s.value(entryKey).toString();
}

void GameVaultSettings::setCustomArtPath(const QString& entryKey, const QString& path) {
    if (entryKey.trimmed().isEmpty()) return;
    QSettings s(kOrg, kApp);
    s.beginGroup(kArtGroup);
    s.setValue(entryKey, path);
}

void GameVaultSettings::clearCustomArtPath(const QString& entryKey) {
    if (entryKey.trimmed().isEmpty()) return;
    QSettings s(kOrg, kApp);
    s.beginGroup(kArtGroup);
    s.remove(entryKey);
}

QVector<GameEntry> GameVaultSettings::manualGames() const {
    QSettings s(kOrg, kApp);
    const QByteArray raw = s.value(kManualGames).toByteArray();
    if (raw.isEmpty()) return {};

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) return {};

    QVector<GameEntry> out;
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        GameEntry game = deserializeManualGame(v.toObject());
        if (game.title.trimmed().isEmpty()) continue;
        out.push_back(std::move(game));
    }
    return out;
}

void GameVaultSettings::setManualGames(const QVector<GameEntry>& games) {
    QJsonArray arr;
    for (const GameEntry& game : games) {
        if (game.title.trimmed().isEmpty()) continue;
        arr.append(serializeManualGame(game));
    }

    QSettings s(kOrg, kApp);
    s.setValue(kManualGames, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void GameVaultSettings::addOrUpdateManualGame(const GameEntry& game) {
    if (game.title.trimmed().isEmpty()) return;

    QVector<GameEntry> all = manualGames();
    const QString key = manualGameKey(game);
    bool updated = false;
    for (GameEntry& existing : all) {
        if (manualGameKey(existing) == key) {
            existing = game;
            updated = true;
            break;
        }
    }
    if (!updated) all.push_back(game);
    setManualGames(all);
}

QString GameVaultSettings::gameExecutableOverridePath(const QString& locatorKey) const {
    const QString key = locatorKey.trimmed().toLower();
    if (key.isEmpty()) return {};

    QSettings s(kOrg, kApp);
    const QByteArray raw = s.value(kGameOverrides).toByteArray();
    if (raw.isEmpty()) return {};

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return {};

    const QJsonObject root = doc.object();
    if (!root.value(key).isObject()) return {};
    return root.value(key).toObject().value("executablePath").toString();
}

QString GameVaultSettings::gameTrackingIdOverride(const QString& locatorKey) const {
    const QString key = locatorKey.trimmed().toLower();
    if (key.isEmpty()) return {};

    QSettings s(kOrg, kApp);
    const QByteArray raw = s.value(kGameOverrides).toByteArray();
    if (raw.isEmpty()) return {};

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return {};

    const QJsonObject root = doc.object();
    if (!root.value(key).isObject()) return {};
    return root.value(key).toObject().value("trackingId").toString();
}

void GameVaultSettings::setGameExecutableOverride(const QString& locatorKey,
                                                  const QString& executablePath,
                                                  const QString& trackingId) {
    const QString key = locatorKey.trimmed().toLower();
    if (key.isEmpty()) return;

    QSettings s(kOrg, kApp);
    QJsonObject root;

    const QByteArray raw = s.value(kGameOverrides).toByteArray();
    const QJsonDocument existing = QJsonDocument::fromJson(raw);
    if (existing.isObject()) {
        root = existing.object();
    }

    QJsonObject value;
    value.insert("executablePath", executablePath);
    value.insert("trackingId", trackingId);
    root.insert(key, value);

    s.setValue(kGameOverrides, QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void GameVaultSettings::clearGameExecutableOverride(const QString& locatorKey) {
    const QString key = locatorKey.trimmed().toLower();
    if (key.isEmpty()) return;

    QSettings s(kOrg, kApp);
    const QByteArray raw = s.value(kGameOverrides).toByteArray();
    if (raw.isEmpty()) return;

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    root.remove(key);
    s.setValue(kGameOverrides, QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString GameVaultSettings::steamGridDbApiKey() const {
    QSettings s(kOrg, kApp);
    return s.value("gamevault/steamgriddb_api_key").toString();
}

void GameVaultSettings::setSteamGridDbApiKey(const QString& key) {
    QSettings s(kOrg, kApp);
    s.setValue("gamevault/steamgriddb_api_key", key.trimmed());
}

}
