#pragma once

#include "logger/logger.hpp"
#include "logger/log_sink.hpp"

#include <QAbstractTableModel>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QVector>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSplitter;
class QTableView;
namespace wintools::themes { class ThemeListener; }

namespace wintools::logviewer {

class LogTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColTime = 0, ColSeverity, ColSource, ColMessage, ColumnCount };

    explicit LogTableModel(QObject* parent = nullptr);

    void setEntries(const QVector<wintools::logger::LogEntry>& entries);
    void appendEntry(const wintools::logger::LogEntry& entry);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<wintools::logger::LogEntry> m_entries;
};

class LogFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit LogFilterProxy(QObject* parent = nullptr);

    void setSourcePrefix(const QString& prefix);
    void setSeverityFilter(int sev);
    void setMessageFilter(const QString& text);

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

private:
    QString m_sourcePrefix;
    int     m_severity{-1};
    QString m_msgFilter;
};

class LogViewerWindow : public QDialog {
    Q_OBJECT
public:
    explicit LogViewerWindow(QWidget* parent = nullptr);
    ~LogViewerWindow() override;

private slots:
    void onNewEntry(const wintools::logger::LogEntry& entry);
    void onSourceSelected(int row);
    void onSeverityFilterChanged(int index);
    void onMessageFilterChanged(const QString& text);
    void clearLog();
    void exportLog();

private:
    void buildUi();
    void buildSidebar();
    void buildToolbar(QWidget* parent);
    void buildTable(QWidget* parent);
    void buildLegend();
    void applyTheme();
    void updateLegendCounts();
    void scrollToBottom();

    QSplitter*          m_splitter       = nullptr;
    QFrame*             m_sidebar        = nullptr;
    QListWidget*        m_sourceList     = nullptr;

    QLineEdit*          m_filterEdit     = nullptr;
    QComboBox*          m_severityCombo  = nullptr;
    QCheckBox*          m_autoScrollChk  = nullptr;
    QPushButton*        m_clearBtn       = nullptr;
    QPushButton*        m_exportBtn      = nullptr;

    QTableView*         m_tableView      = nullptr;
    LogTableModel*      m_model          = nullptr;
    LogFilterProxy*     m_proxy          = nullptr;

    QFrame*             m_legend         = nullptr;
    QLabel*             m_lblTotal       = nullptr;
    QLabel*             m_lblShowing     = nullptr;
    QLabel*             m_lblErrors      = nullptr;
    QLabel*             m_lblWarnings    = nullptr;
    QLabel*             m_lblPasses      = nullptr;

    bool                m_autoScroll{true};
    wintools::themes::ThemeListener* m_themeListener = nullptr;
};

}
