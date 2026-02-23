#include "common/ui/launch_page_section_factory.hpp"

#include <QCheckBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

// WinTools: launch page section factory manages UI behavior and presentation.

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
    const wintools::themes::ThemePalette& palette) {
    table->clearContents();

    QStringList names;
    QStringList hotkeys;

    for (const auto& module : modules) {
        if (!module.enabled) {
            continue;
        }
        names.push_back(normalizeName(module.name));
        hotkeys.push_back(hotkeyDisplay.value(module.name, "-"));
    }

    if (names.isEmpty()) {
        for (int i = 0; i < 6; ++i) {
            names.push_back("null");
            hotkeys.push_back("null");
        }
    }

    table->setRowCount(names.size());
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Module", "Hotkey  (click to set, right-click for more)"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    for (int i = 0; i < names.size(); ++i) {
        auto* nameItem = new QTableWidgetItem(names[i]);
        nameItem->setFlags(Qt::ItemIsEnabled);

        auto* hotkeyItem = new QTableWidgetItem(hotkeys[i]);
        hotkeyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        hotkeyItem->setToolTip("Left-click to set hotkey  |  Right-click for advanced options");
        hotkeyItem->setTextAlignment(Qt::AlignCenter);

        table->setItem(i, 0, nameItem);
        table->setItem(i, 1, hotkeyItem);
    }

    table->setStyleSheet(QString(
        "QTableWidget{"
        "  border:1px solid %1;"
        "  border-radius: 6px;"
        "  background:%2;"
        "  color:%3;"
        "  gridline-color: rgba(%4,%5,%6,40);"
        "}"
        "QHeaderView::section{"
        "  background:%2;"
        "  color:%7;"
        "  border:none;"
        "  border-right:1px solid %1;"
        "  border-bottom:1px solid %1;"
        "  padding: 6px 10px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "}"
    ).arg(
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.foreground.name(),
        QString::number(palette.cardBorder.red()),
        QString::number(palette.cardBorder.green()),
        QString::number(palette.cardBorder.blue()),
        palette.mutedForeground.name()
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

    const int rows = modules.empty() ? 8 : static_cast<int>(modules.size());
    table->setRowCount(rows);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"Module", "Enabled", ">"});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    if (modules.empty()) {
        for (int i = 0; i < 8; ++i) {
            table->setItem(i, 0, new QTableWidgetItem("null"));
            table->setItem(i, 1, new QTableWidgetItem("null"));
            table->setItem(i, 2, new QTableWidgetItem(">"));
        }
    } else {
        for (int i = 0; i < rows; ++i) {
            auto& module = modules[static_cast<size_t>(i)];
            table->setItem(i, 0, new QTableWidgetItem(normalizeName(module.name)));

            auto* toggle = new QCheckBox(module.enabled ? "ON" : "OFF");
            toggle->setChecked(module.enabled);
            toggle->setStyleSheet(QString("QCheckBox{color:%1;} QCheckBox::indicator{width:16px;height:16px;}")
                                      .arg(palette.foreground.name()));

            QObject::connect(toggle, &QCheckBox::toggled, [i, &modules, toggle, onModuleToggled](bool checked) {
                auto& changed = modules[static_cast<size_t>(i)];
                changed.enabled = checked;
                toggle->setText(checked ? "ON" : "OFF");
                onModuleToggled();
            });

            table->setCellWidget(i, 1, toggle);
            table->setItem(i, 2, new QTableWidgetItem(">"));
        }
    }

    table->setStyleSheet(QString(
        "QTableWidget{"
        "  border:1px solid %1;"
        "  border-radius: 6px;"
        "  background:%2;"
        "  color:%3;"
        "  gridline-color: rgba(%4,%5,%6,40);"
        "}"
        "QHeaderView::section{"
        "  background:%2;"
        "  color:%7;"
        "  border:none;"
        "  border-right:1px solid %1;"
        "  border-bottom:1px solid %1;"
        "  padding: 6px 10px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "}"
    ).arg(
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.foreground.name(),
        QString::number(palette.cardBorder.red()),
        QString::number(palette.cardBorder.green()),
        QString::number(palette.cardBorder.blue()),
        palette.mutedForeground.name()
    ));
}

}
