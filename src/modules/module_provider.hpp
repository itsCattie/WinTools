#pragma once

#include <functional>
#include <vector>

#include <QHash>
#include <QString>

class QWidget;

namespace wintools::modules {

struct ModuleEntry {
    QString name;
    QString iconPath;
    bool    enabled = true;
    std::function<void(QWidget*)>                 launch;
    QHash<QString, std::function<void()>>         actions;
};

class ModuleProvider {
public:
    static std::vector<ModuleEntry> loadModules();
};

}
