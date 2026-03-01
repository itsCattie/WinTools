#pragma once

#include "common/themes/window_colour.hpp"
#include "modules/module_provider.hpp"

#include <functional>
#include <vector>

#include <QHash>
#include <QString>

class QWidget;
class QTableWidget;
class QGridLayout;

namespace wintools::ui {

class LaunchPageSectionFactory {
public:
    static void buildQuickAccess(QWidget* host, QGridLayout* layout, std::vector<wintools::modules::ModuleEntry>& modules, const wintools::themes::ThemePalette& palette);

    static void buildHotkeys(QTableWidget* table,
                             const std::vector<wintools::modules::ModuleEntry>& modules,
                             const QHash<QString, QString>& hotkeyDisplay,
                             const wintools::themes::ThemePalette& palette,
                             const std::function<void(const QString&, const QString&)>& onEditHotkey);
    static void buildModules(QTableWidget* table, std::vector<wintools::modules::ModuleEntry>& modules, const wintools::themes::ThemePalette& palette, const std::function<void()>& onModuleToggled, QWidget* parent);
};

}
