#include "common/themes/fluent_style.hpp"

// WinTools: fluent style manages shared infrastructure.

namespace wintools::themes {

static inline QString rgba(const QColor& c, int alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

static inline QString hex(const QColor& c) {
    return c.name();
}

QString FluentStyle::cardStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QGroupBox {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  margin-top: 14px;"
        "  padding: 16px 12px 12px 12px;"
        "  color: %3;"
        "  font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 14px;"
        "  padding: 0 6px;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  color: %3;"
        "}"
    ).arg(hex(p.cardBackground), hex(p.cardBorder), hex(p.foreground));
}

QString FluentStyle::buttonStyle(const ThemePalette& p) {

    const QColor hover = p.cardBorder;
    const QColor press = p.mutedForeground;

    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  color: %3;"
        "  font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  min-height: 22px;"
        "}"
        "QPushButton:hover {"
        "  background: %4;"
        "  border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "  background: %6;"
        "  border-color: %6;"
        "  color: %7;"
        "}"
        "QPushButton:disabled {"
        "  background: %1;"
        "  color: %8;"
        "  border-color: %2;"
        "}"
    ).arg(
        hex(p.cardBackground),
        hex(p.cardBorder),
        hex(p.foreground),
        rgba(hover, 80),
        hex(p.accent),
        rgba(press, 60),
        hex(p.foreground),
        hex(p.mutedForeground)
    );
}

QString FluentStyle::inputStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QLineEdit {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  color: %3;"
        "  font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  selection-background-color: %4;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %4;"
        "  border-bottom: 2px solid %4;"
        "}"
        "QComboBox {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  color: %3;"
        "  font-size: 13px;"
        "}"
        "QComboBox:hover {"
        "  border-color: %4;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 24px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  selection-background-color: %5;"
        "  color: %3;"
        "}"
    ).arg(
        hex(p.cardBackground),
        hex(p.cardBorder),
        hex(p.foreground),
        hex(p.accent),
        rgba(p.accent, 50)
    );
}

QString FluentStyle::sidebarStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QFrame#FluentSidebar {"
        "  background: %1;"
        "  border-right: 1px solid %2;"
        "}"
        "QFrame#FluentSidebar QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  text-align: left;"
        "  color: %3;"
        "  font-size: 13px;"
        "}"
        "QFrame#FluentSidebar QPushButton:hover {"
        "  background: %4;"
        "}"
        "QFrame#FluentSidebar QPushButton:pressed {"
        "  background: %5;"
        "}"
        "QFrame#FluentSidebar QPushButton:checked {"
        "  background: %6;"
        "  color: %7;"
        "  font-weight: 600;"
        "}"
    ).arg(
        hex(p.cardBackground),
        hex(p.cardBorder),
        hex(p.foreground),
        rgba(p.foreground, 15),
        rgba(p.foreground, 25),
        rgba(p.accent, 30),
        hex(p.accent)
    );
}

QString FluentStyle::tabStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QTabWidget::pane {"
        "  border: 1px solid %1;"
        "  border-radius: 8px;"
        "  background: %2;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: %3;"
        "  padding: 8px 20px;"
        "  margin-right: 2px;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "  font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "}"
        "QTabBar::tab:hover {"
        "  background: %4;"
        "}"
        "QTabBar::tab:selected {"
        "  background: %2;"
        "  color: %5;"
        "  font-weight: 600;"
        "  border-bottom: 2px solid %6;"
        "}"
    ).arg(
        hex(p.cardBorder),
        hex(p.windowBackground),
        hex(p.mutedForeground),
        rgba(p.foreground, 10),
        hex(p.foreground),
        hex(p.accent)
    );
}

QString FluentStyle::tableStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QTreeView, QTableView, QTableWidget {"
        "  background: %1;"
        "  alternate-background-color: %2;"
        "  color: %3;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "  gridline-color: %5;"
        "  selection-background-color: %6;"
        "  selection-color: %7;"
        "  outline: 0;"
        "  font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "}"
        "QHeaderView::section {"
        "  background: %2;"
        "  color: %8;"
        "  padding: 6px 10px;"
        "  border: none;"
        "  border-right: 1px solid %4;"
        "  border-bottom: 1px solid %4;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "}"
        "QHeaderView::section:hover {"
        "  background: %9;"
        "}"
    ).arg(
        hex(p.windowBackground),
        rgba(p.cardBackground, 255),
        hex(p.foreground),
        hex(p.cardBorder),
        rgba(p.cardBorder, 80),
        rgba(p.accent, 60),
        hex(p.foreground)
    ).arg(
        hex(p.mutedForeground),
        rgba(p.foreground, 15)
    );
}

QString FluentStyle::toggleStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QCheckBox {"
        "  spacing: 8px;"
        "  color: %1;"
        "  font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 40px;"
        "  height: 20px;"
        "  border-radius: 10px;"
        "  border: 2px solid %2;"
        "  background: %3;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: %4;"
        "  border-color: %4;"
        "}"
    ).arg(
        hex(p.foreground),
        hex(p.cardBorder),
        hex(p.cardBackground),
        hex(p.accent)
    );
}

QString FluentStyle::scrollbarStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 8px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %1;"
        "  min-height: 30px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: %2;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 8px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: %1;"
        "  min-width: 30px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: %2;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0;"
        "}"
    ).arg(
        rgba(p.mutedForeground, 80),
        rgba(p.mutedForeground, 140)
    );
}

QString FluentStyle::tooltipStyle(const ThemePalette& p) {
    return QStringLiteral(
        "QToolTip {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 12px;"
        "}"
    ).arg(hex(p.cardBackground), hex(p.foreground), hex(p.cardBorder));
}

QString FluentStyle::generate(const ThemePalette& p) {
    return QStringLiteral(
        "* { font-family: 'Segoe UI Variable', 'Segoe UI', sans-serif; }"
        "QMainWindow { background: %1; color: %2; }"
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %2; }"
        "QFrame { color: %2; }"
        "QMenu {"
        "  background: %3;"
        "  border: 1px solid %4;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  color: %2;"
        "}"
        "QMenu::item {"
        "  padding: 6px 24px 6px 12px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background: %5;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: %4;"
        "  margin: 4px 8px;"
        "}"
    ).arg(
        hex(p.windowBackground),
        hex(p.foreground),
        hex(p.cardBackground),
        hex(p.cardBorder),
        rgba(p.foreground, 15)
    )
    + cardStyle(p)
    + buttonStyle(p)
    + inputStyle(p)
    + tabStyle(p)
    + tableStyle(p)
    + toggleStyle(p)
    + scrollbarStyle(p)
    + tooltipStyle(p);
}

}
