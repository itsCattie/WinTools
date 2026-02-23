#pragma once

// AdvancedTaskManager: process model manages model/view data shaping.

#include <QAbstractItemModel>
#include <QVector>
#include <QSortFilterProxyModel>

#include "modules/AdvancedTaskManager/src/core/process_info.hpp"

namespace wintools::taskmanager {

namespace ProcessCol {
    inline constexpr int Name     = 0;
    inline constexpr int PID      = 1;
    inline constexpr int Status   = 2;
    inline constexpr int CPU      = 3;
    inline constexpr int Memory   = 4;
    inline constexpr int Disk     = 5;
    inline constexpr int Handles  = 6;
    inline constexpr int Threads  = 7;
    inline constexpr int Username = 8;
    inline constexpr int Count    = 9;
}

enum ProcessRole {
    PidRole      = Qt::UserRole + 1,
    CategoryRole = Qt::UserRole + 2,
    IsGroupRole  = Qt::UserRole + 3,
    RawCpuRole   = Qt::UserRole + 4,
    RawMemRole   = Qt::UserRole + 5,
    RawDiskRole  = Qt::UserRole + 6,
};

class ProcessTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit ProcessTreeModel(QObject* parent = nullptr);

    void update(const QVector<ProcessInfo>& processes);

    const ProcessInfo* infoAt(const QModelIndex& idx) const;

    QModelIndex   index(int row, int column,
                        const QModelIndex& parent = {}) const override;
    QModelIndex   parent(const QModelIndex& child) const override;
    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    static constexpr int kGroupCount = 3;
    static constexpr quint32 kGroupSentinel = 0xFFFFFFFFu;

    QVector<ProcessInfo> m_groups[kGroupCount];

    static QString groupLabel(int g);
    static QString formatMemory(quint64 bytes);
    static QString formatDisk(quint64 readBps, quint64 writeBps);
    static QString statusText(ProcessStatus s);
    static QString priorityText(int cls);
};

class ProcessTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ProcessTableModel(QObject* parent = nullptr);

    void update(const QVector<ProcessInfo>& processes);
    const ProcessInfo* infoAt(int row) const;

    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    QVector<ProcessInfo> m_rows;

    static QString formatMemory(quint64 bytes);
    static QString formatDisk(quint64 readBps, quint64 writeBps);
    static QString statusText(ProcessStatus s);
    static QString priorityText(int cls);
};

class ProcessFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit ProcessFilterProxy(QObject* parent = nullptr);
    void setFilterText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_filterText;
};

}
