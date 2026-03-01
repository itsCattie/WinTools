#include "common/ui/launch_page_section_factory.hpp"
#include "common/ui/hotkey_bind_dialog.hpp"

#include <QCheckBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace wintools::ui {

namespace {
QString normalizeName(const QString& value) {
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? "null" : trimmed;
}

void clearGrid(QGridLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
}

void LaunchPageSectionFactory::buildQuickAccess(
    QWidget* host,
    QGridLayout* layout,
    std::vector<wintools::modules::ModuleEntry>& modules,
    const wintools::themes::ThemePalette& palette) {
    Q_UNUSED(host);
    clearGrid(layout);

    std::vector<int> enabledIndexes;
    for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
        if (modules[static_cast<size_t>(i)].enabled) {
            enabledIndexes.push_back(i);
        }
    }

    QStringList names;
    for (int index : enabledIndexes) {
        names.push_back(normalizeName(modules[static_cast<size_t>(index)].name));
    }

    if (names.isEmpty()) {
        for (int i = 0; i < 6; ++i) {
            names.push_back("null");
        }
    }

    for (int i = 0; i < names.size(); ++i) {
        auto* button = new QPushButton(names[i]);
        button->setEnabled(names[i].compare("null", Qt::CaseInsensitive) != 0);

        if (i < static_cast<int>(enabledIndexes.size())) {
            const auto& mod = modules[static_cast<size_t>(enabledIndexes[i])];
            if (!mod.iconPath.isEmpty()) {
                button->setIcon(QIcon(mod.iconPath));
                button->setIconSize(QSize(24, 24));
            }
        }

        button->setMinimumHeight(48);
        button->setStyleSheet(QString(
            "QPushButton{"
            "  padding: 10px 14px;"
            "  border: 1px solid %1;"
            "  border-radius: 8px;"
            "  background: %2;"
            "  color: %3;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "  text-align: left;"
            "}"
            "QPushButton:hover{"
            "  background: %4;"
            "  border-color: %5;"
            "}"
            "QPushButton:pressed{"
            "  background: %6;"
            "}"
        ).arg(
            palette.cardBorder.name(),
            palette.cardBackground.name(),
            palette.foreground.name(),
            QStringLiteral("rgba(%1,%2,%3,15)").arg(palette.foreground.red()).arg(palette.foreground.green()).arg(palette.foreground.blue()),
            palette.accent.name(),
            QStringLiteral("rgba(%1,%2,%3,25)").arg(palette.foreground.red()).arg(palette.foreground.green()).arg(palette.foreground.blue())
        ));

        if (i < static_cast<int>(enabledIndexes.size())) {
            const int moduleIndex = enabledIndexes[i];
            QObject::connect(button, &QPushButton::clicked, [moduleIndex, &modules]() {
                auto& module = modules[static_cast<size_t>(moduleIndex)];
                if (module.launch) {
                    module.launch(nullptr);
                }
            });
        }

        layout->addWidget(button, i / 3, i % 3);
    }
}

void LaunchPageSectionFactory::buildHotkeys(
    QTableWidget* table,
    const std::vector<wintools::modules::ModuleEntry>& modules,
    const QHash<QString, QString>& hotkeyDisplay,
    const wintools::themes::ThemePalette& palette,
    const std::function<void(const QString&, const QString&)>& onEditHotkey) {
    table->clearContents();
    table->setAccessibleName("Hotkeys table");
    table->setAccessibleDescription("Shows module actions and their hotkeys. Use arrow keys to move rows and press Enter to edit.");

    struct HotkeyRow {
        QString module;
        QString action;
        QString moduleLabel;
        QString actionLabel;
        QString hotkey;
    };

    std::vector<HotkeyRow> rows;

    for (const auto& module : modules) {
        if (!module.enabled) continue;

        const QString moduleName = normalizeName(module.name);
        const auto addRow = [&](const QString& actionId) {
            const QString key = QString("%1.%2").arg(module.name, actionId);
            rows.push_back(HotkeyRow{
                .module = module.name,
                .action = actionId,
                .moduleLabel = moduleName,
                .actionLabel = HotkeyBindDialog::actionIdToLabel(actionId),
                .hotkey = hotkeyDisplay.value(key, "Not set")
            });
        };

        addRow("toggle");
        for (auto it = module.actions.cbegin(); it != module.actions.cend(); ++it) {
            if (it.key() == "toggle") continue;
            addRow(it.key());
        }
    }

    if (rows.empty()) {
        table->setColumnCount(3);
        table->setHorizontalHeaderLabels({"Module", "Action", "Shortcut"});
        table->setRowCount(1);
        auto* empty = new QTableWidgetItem("No enabled modules");
        empty->setFlags(Qt::ItemIsEnabled);
        table->setItem(0, 0, empty);
        table->setItem(0, 1, new QTableWidgetItem("-"));
        table->setItem(0, 2, new QTableWidgetItem("-"));
    } else {
        table->setRowCount(static_cast<int>(rows.size()));
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({"Module", "Action", "Shortcut", "Edit"});
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            const HotkeyRow& row = rows[static_cast<size_t>(i)];

            auto* moduleItem = new QTableWidgetItem(row.moduleLabel);
            moduleItem->setData(Qt::UserRole, row.module);
            moduleItem->setData(Qt::UserRole + 1, row.action);
            moduleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            auto* actionItem = new QTableWidgetItem(row.actionLabel);
            actionItem->setData(Qt::UserRole, row.module);
            actionItem->setData(Qt::UserRole + 1, row.action);
            actionItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            auto* hotkeyItem = new QTableWidgetItem(row.hotkey);
            hotkeyItem->setData(Qt::UserRole, row.module);
            hotkeyItem->setData(Qt::UserRole + 1, row.action);
            hotkeyItem->setToolTip("Select row and press Enter, or click Edit");
            hotkeyItem->setTextAlignment(Qt::AlignCenter);
            hotkeyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (row.hotkey.compare("Not set", Qt::CaseInsensitive) == 0) {
                hotkeyItem->setForeground(palette.mutedForeground);
            }

            auto* changeButton = new QPushButton("Edit");
            changeButton->setCursor(Qt::PointingHandCursor);
            changeButton->setMinimumHeight(28);
            changeButton->setMinimumWidth(74);
            changeButton->setToolTip(QString("Set hotkey for %1 / %2")
                                         .arg(row.moduleLabel, row.actionLabel));
            changeButton->setAccessibleName(QString("Edit hotkey %1 %2")
                                                .arg(row.moduleLabel, row.actionLabel));
            QObject::connect(changeButton, &QPushButton::clicked,
                             [onEditHotkey, module = row.module, action = row.action]() {
                                 onEditHotkey(module, action);
                             });

            table->setItem(i, 0, moduleItem);
            table->setItem(i, 1, actionItem);
            table->setItem(i, 2, hotkeyItem);
            table->setCellWidget(i, 3, changeButton);
        }
    }

    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::StrongFocus);
    table->setTabKeyNavigation(true);
    table->setWordWrap(false);
    table->horizontalHeader()->setHighlightSections(false);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    if (rows.empty()) {
        table->horizontalHeader()->setStretchLastSection(true);
    } else {
        for (int row = 0; row < table->rowCount(); ++row) {
            table->setRowHeight(row, 38);
        }
    }

    table->setStyleSheet(QString(
        "QTableWidget{"
        "  border:1px solid %1;"
        "  border-radius: 8px;"
        "  background:%2;"
        "  color:%3;"
        "  alternate-background-color: rgba(%4,%5,%6,14);"
        "  selection-background-color: rgba(%9,%10,%11,32);"
        "  selection-color:%3;"
        "}"
        "QHeaderView::section{"
        "  background:%2;"
        "  color:%7;"
        "  border:none;"
        "  border-bottom:1px solid %1;"
        "  padding: 8px 10px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  letter-spacing: 0.3px;"
        "}"
        "QTableWidget::item{"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid rgba(%4,%5,%6,20);"
        "}"
        "QPushButton{"
        "  border:1px solid %1;"
        "  border-radius:6px;"
        "  padding:5px 12px;"
        "  background:%2;"
        "  color:%3;"
        "}"
        "QPushButton:hover{"
        "  border-color:%8;"
        "  background: rgba(%9,%10,%11,16);"
        "}"
        "QPushButton:focus{"
        "  border-color:%8;"
        "  background: rgba(%9,%10,%11,20);"
        "}"
    ).arg(
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.foreground.name(),
        QString::number(palette.cardBorder.red()),
        QString::number(palette.cardBorder.green()),
        QString::number(palette.cardBorder.blue()),
        palette.mutedForeground.name(),
        palette.accent.name(),
        QString::number(palette.accent.red()),
        QString::number(palette.accent.green()),
        QString::number(palette.accent.blue())
    ));
}

void LaunchPageSectionFactory::buildModules(
    QTableWidget* table,
    std::vector<wintools::modules::ModuleEntry>& modules,
    const wintools::themes::ThemePalette& palette,
    const std::function<void()>& onModuleToggled,
    QWidget* parent) {
    Q_UNUSED(parent);
    table->clearContents();

    table->setAccessibleName("Modules table");
    table->setAccessibleDescription("Shows available modules and whether each one is enabled in quick access and tray.");

    const int rows = modules.empty() ? 1 : static_cast<int>(modules.size());
    table->setRowCount(rows);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"Module", "Status", "Enabled"});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::StrongFocus);
    table->setWordWrap(false);
    table->horizontalHeader()->setHighlightSections(false);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    if (modules.empty()) {
        auto* moduleItem = new QTableWidgetItem("No modules available");
        moduleItem->setFlags(Qt::ItemIsEnabled);
        moduleItem->setForeground(palette.mutedForeground);

        auto* statusItem = new QTableWidgetItem("-");
        statusItem->setFlags(Qt::ItemIsEnabled);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(palette.mutedForeground);

        auto* enabledItem = new QTableWidgetItem("-");
        enabledItem->setFlags(Qt::ItemIsEnabled);
        enabledItem->setTextAlignment(Qt::AlignCenter);
        enabledItem->setForeground(palette.mutedForeground);

        table->setItem(0, 0, moduleItem);
        table->setItem(0, 1, statusItem);
        table->setItem(0, 2, enabledItem);
    } else {
        for (int i = 0; i < rows; ++i) {
            auto& module = modules[static_cast<size_t>(i)];

            auto* moduleItem = new QTableWidgetItem(normalizeName(module.name));
            moduleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (!module.iconPath.isEmpty()) {
                moduleItem->setIcon(QIcon(module.iconPath));
            }

            auto* statusItem = new QTableWidgetItem(module.enabled ? "Enabled" : "Disabled");
            statusItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            statusItem->setTextAlignment(Qt::AlignCenter);
            statusItem->setForeground(module.enabled ? palette.foreground : palette.mutedForeground);

            auto* toggle = new QCheckBox(module.enabled ? "On" : "Off");
            toggle->setChecked(module.enabled);
            toggle->setCursor(Qt::PointingHandCursor);
            toggle->setAccessibleName(QString("Toggle module %1").arg(module.name));
            toggle->setStyleSheet(QString(
                "QCheckBox{color:%1; spacing: 6px; font-weight: 600;}"
                "QCheckBox::indicator{width:16px;height:16px;}"
            ).arg(palette.foreground.name()));

            QObject::connect(toggle, &QCheckBox::toggled,
                             [i, &modules, toggle, statusItem, onModuleToggled, palette](bool checked) {
                                 auto& changed = modules[static_cast<size_t>(i)];
                                 changed.enabled = checked;
                                 toggle->setText(checked ? "On" : "Off");
                                 statusItem->setText(checked ? "Enabled" : "Disabled");
                                 statusItem->setForeground(checked ? palette.foreground : palette.mutedForeground);
                                 onModuleToggled();
                             });

            table->setItem(i, 0, moduleItem);
            table->setItem(i, 1, statusItem);
            table->setCellWidget(i, 2, toggle);
        }
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        table->setRowHeight(row, 38);
    }

    table->setStyleSheet(QString(
        "QTableWidget{"
        "  border:1px solid %1;"
        "  border-radius: 8px;"
        "  background:%2;"
        "  color:%3;"
        "  alternate-background-color: rgba(%4,%5,%6,14);"
        "  selection-background-color: rgba(%8,%9,%10,30);"
        "  selection-color:%3;"
        "}"
        "QHeaderView::section{"
        "  background:%2;"
        "  color:%7;"
        "  border:none;"
        "  border-bottom:1px solid %1;"
        "  padding: 8px 10px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  letter-spacing: 0.3px;"
        "}"
        "QTableWidget::item{"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid rgba(%4,%5,%6,20);"
        "}"
        "QCheckBox{"
        "  margin-left: 6px;"
        "}"
        "QCheckBox::indicator{"
        "  border:1px solid %1;"
        "  border-radius: 3px;"
        "  background:%2;"
        "}"
        "QCheckBox::indicator:checked{"
        "  background:%11;"
        "  border-color:%11;"
        "}"
    ).arg(
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.foreground.name(),
        QString::number(palette.cardBorder.red()),
        QString::number(palette.cardBorder.green()),
        QString::number(palette.cardBorder.blue()),
        palette.mutedForeground.name(),
        QString::number(palette.accent.red()),
        QString::number(palette.accent.green()),
        QString::number(palette.accent.blue()),
        palette.accent.name()
    ));
}

}
