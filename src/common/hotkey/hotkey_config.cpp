#include "common/hotkey/hotkey_config.hpp"

#include "logger/logger.hpp"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace wintools::hotkeys {

namespace {
constexpr const char* LogSource = "HotkeyConfig";
}

QString HotkeyConfig::configPath() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/hotkeys.json";
}

QList<HotkeyBinding> HotkeyConfig::defaults() {
    return {

        HotkeyBinding{
            .action    = { "mediabar.toggle",    "MediaBar", "toggle" },
            .keyString = "F21",
            .modifiers = {},
            .enabled   = true,
        },

        HotkeyBinding{
            .action    = { "mediabar.show_mini", "MediaBar", "show_mini" },
            .keyString = "F22",
            .modifiers = {},
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "mediabar.show_full", "MediaBar", "show_full" },
            .keyString = "F22",
            .modifiers = { "Shift" },
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "wintools.open_main", "", "open_main" },
            .keyString = "F24",
            .modifiers = {},
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "disksentinel.open", "DiskSentinel", "open" },
            .keyString = "F23",
            .modifiers = {},
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "taskmanager.open", "AdvancedTaskManager", "open" },
            .keyString = "T",
            .modifiers = { "Ctrl", "Alt" },
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "taskmanager.toggle_profiler", "AdvancedTaskManager", "toggle_profiler" },
            .keyString = "P",
            .modifiers = { "Ctrl", "Alt" },
            .enabled   = true,
        },

        HotkeyBinding{
            .action    = { "gamevault.open", "GameVault", "open" },
            .keyString = "G",
            .modifiers = { "Ctrl", "Alt" },
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "streamvault.open", "StreamVault", "open" },
            .keyString = "S",
            .modifiers = { "Ctrl", "Alt" },
            .enabled   = false,
        },

        HotkeyBinding{
            .action    = { "logviewer.open", "LogViewer", "open" },
            .keyString = "L",
            .modifiers = { "Ctrl", "Alt" },
            .enabled   = false,
        },
    };
}

QList<HotkeyBinding> HotkeyConfig::load() {
    const QString path = configPath();

    if (!QFile::exists(path)) {
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                      "No hotkey config found – writing defaults.");
        const auto def = defaults();
        save(def);
        return def;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning,
                                      "Cannot read hotkey config – using built-in defaults.");
        return defaults();
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (doc.isNull()) {
        wintools::logger::Logger::log(
            LogSource, wintools::logger::Severity::Warning,
            QString("JSON parse error in hotkey config: %1 – using built-in defaults.")
                .arg(err.errorString()));
        return defaults();
    }

    QList<HotkeyBinding> result;
    const QJsonArray arr = doc.object().value("bindings").toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject obj = v.toObject();

        HotkeyBinding b;
        b.action.id     = obj.value("id").toString();
        b.action.module = obj.value("module").toString();
        b.action.action = obj.value("action").toString();
        b.keyString     = obj.value("key").toString();
        b.enabled       = obj.value("enabled").toBool(true);

        const QJsonArray modArr = obj.value("modifiers").toArray();
        for (const QJsonValue& m : modArr)
            b.modifiers.append(m.toString());

        result.append(b);
    }

    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  QString("Loaded %1 hotkey binding(s) from disk.")
                                      .arg(result.size()));

    bool added = false;
    for (const HotkeyBinding& def : defaults()) {
        const bool exists = std::any_of(result.cbegin(), result.cend(),
            [&](const HotkeyBinding& b) { return b.action.id == def.action.id; });
        if (!exists) {
            result.append(def);
            added = true;
        }
    }
    if (added) {
        save(result);
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                      "Merged new default hotkey bindings into saved config.");
    }

    return result;
}

void HotkeyConfig::save(const QList<HotkeyBinding>& bindings) {
    QJsonArray arr;
    for (const HotkeyBinding& b : bindings) {
        QJsonObject obj;
        obj["id"]      = b.action.id;
        obj["module"]  = b.action.module;
        obj["action"]  = b.action.action;
        obj["key"]     = b.keyString;
        obj["enabled"] = b.enabled;

        QJsonArray modArr;
        for (const QString& m : b.modifiers) modArr.append(m);
        obj["modifiers"] = modArr;

        arr.append(obj);
    }

    QJsonObject root;
    root["bindings"] = arr;

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Warning,
                                      "Cannot write hotkey config to disk.");
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  QString("Saved %1 hotkey binding(s).").arg(bindings.size()));
}

}
