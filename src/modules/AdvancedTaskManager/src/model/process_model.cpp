#include "modules/AdvancedTaskManager/src/model/process_model.hpp"

#include <QFont>
#include <QBrush>
#include <QColor>
#include <algorithm>

namespace wintools::taskmanager {

static QString fmtMemory(quint64 bytes) {
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1024 * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024 * 1024));
    return QStringLiteral("%1 GB").arg(
        static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

static QString fmtDisk(quint64 readBps, quint64 writeBps) {
    quint64 total = readBps + writeBps;
    if (total == 0) return QStringLiteral("—");
    if (total < 1024)
        return QStringLiteral("%1 B/s").arg(total);
    if (total < 1024 * 1024)
        return QStringLiteral("%1 KB/s").arg(total / 1024);
    return QStringLiteral("%1 MB/s").arg(total / (1024 * 1024));
}

static QString fmtStatus(ProcessStatus s) {
    switch (s) {
        case ProcessStatus::Running:      return QStringLiteral("Running");
        case ProcessStatus::Suspended:    return QStringLiteral("Suspended");
        case ProcessStatus::NotResponding: return QStringLiteral("Not responding");
        default:                          return {};
    }
}

static QString fmtPriority(int cls) {
    switch (static_cast<unsigned>(cls)) {
        case 0x00000040: return QStringLiteral("Idle");
        case 0x00004000: return QStringLiteral("Below Normal");
        case 0x00000020: return QStringLiteral("Normal");
        case 0x00008000: return QStringLiteral("Above Normal");
        case 0x00000080: return QStringLiteral("High");
        case 0x00000100: return QStringLiteral("Real-time");
        default:         return QStringLiteral("Normal");
    }
}

ProcessTreeModel::ProcessTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{}

QString ProcessTreeModel::groupLabel(int g) {
    switch (g) {
        case 0:  return QStringLiteral("Apps");
        case 1:  return QStringLiteral("Background Processes");
        case 2:  return QStringLiteral("Windows Processes");
        default: return {};
    }
}
QString ProcessTreeModel::formatMemory(quint64 b)                     { return fmtMemory(b); }
QString ProcessTreeModel::formatDisk(quint64 r, quint64 w)            { return fmtDisk(r, w); }
QString ProcessTreeModel::statusText(ProcessStatus s)                  { return fmtStatus(s); }
QString ProcessTreeModel::priorityText(int cls)                        { return fmtPriority(cls); }

void ProcessTreeModel::update(const QVector<ProcessInfo>& processes) {

    QVector<ProcessInfo> newGroups[kGroupCount];
    for (const auto& pi : processes) {
        int g = static_cast<int>(pi.category);
        if (g >= 0 && g < kGroupCount)
            newGroups[g].push_back(pi);
    }

    for (int g = 0; g < kGroupCount; ++g) {
        auto& oldList = m_groups[g];
        auto& newList = newGroups[g];

        QHash<quint32, int> oldPidIdx;
        for (int i = 0; i < oldList.size(); ++i)
            oldPidIdx.insert(oldList[i].pid, i);

        QHash<quint32, int> newPidIdx;
        for (int i = 0; i < newList.size(); ++i)
            newPidIdx.insert(newList[i].pid, i);

        QModelIndex parent = createIndex(g, 0, kGroupSentinel);

        for (int i = oldList.size() - 1; i >= 0; --i) {
            if (!newPidIdx.contains(oldList[i].pid)) {
                beginRemoveRows(parent, i, i);
                oldList.remove(i);
                endRemoveRows();
            }
        }

        QHash<quint32, int> currentPidIdx;
        for (int i = 0; i < oldList.size(); ++i)
            currentPidIdx.insert(oldList[i].pid, i);

        for (int i = 0; i < oldList.size(); ++i) {
            auto it = newPidIdx.find(oldList[i].pid);
            if (it != newPidIdx.end()) {
                oldList[i] = newList[it.value()];
                emit dataChanged(createIndex(i, 0, static_cast<quintptr>(g)),
                                 createIndex(i, ProcessCol::Count - 1, static_cast<quintptr>(g)));
            }
        }

        QVector<ProcessInfo> toAdd;
        for (const auto& pi : newList) {
            if (!currentPidIdx.contains(pi.pid))
                toAdd.push_back(pi);
        }
        if (!toAdd.isEmpty()) {
            int first = oldList.size();
            int last  = first + toAdd.size() - 1;
            beginInsertRows(parent, first, last);
            oldList.append(toAdd);
            endInsertRows();
        }
    }

    for (int g = 0; g < kGroupCount; ++g) {
        QModelIndex groupIdx = createIndex(g, 0, kGroupSentinel);
        emit dataChanged(groupIdx, groupIdx);
    }
}

const ProcessInfo* ProcessTreeModel::infoAt(const QModelIndex& idx) const {
    if (!idx.isValid()) return nullptr;
    if (idx.internalId() == kGroupSentinel) return nullptr;
    int g = static_cast<int>(idx.internalId());
    int row = idx.row();
    if (g < 0 || g >= kGroupCount) return nullptr;
    if (row < 0 || row >= m_groups[g].size()) return nullptr;
    return &m_groups[g][row];
}

QModelIndex ProcessTreeModel::index(int row, int column,
                                    const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent))
        return {};

    if (!parent.isValid()) {

        return createIndex(row, column, kGroupSentinel);
    }

    int g = parent.row();
    return createIndex(row, column, static_cast<quintptr>(g));
}

QModelIndex ProcessTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return {};
    if (child.internalId() == kGroupSentinel) return {};
    int g = static_cast<int>(child.internalId());
    return createIndex(g, 0, kGroupSentinel);
}

int ProcessTreeModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid())
        return kGroupCount;
    if (parent.internalId() == kGroupSentinel)
        return m_groups[parent.row()].size();
    return 0;
}

int ProcessTreeModel::columnCount(const QModelIndex&) const {
    return ProcessCol::Count;
}

QVariant ProcessTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};

    if (index.internalId() == kGroupSentinel) {
        int g = index.row();
        if (g < 0 || g >= kGroupCount) return {};

        if (role == IsGroupRole) return true;
        if (role == Qt::FontRole) {
            QFont f; f.setBold(true); return f;
        }
        if (role == Qt::ForegroundRole)
            return QBrush(QColor(0x36, 0x93, 0xEA));
        if (role == Qt::DisplayRole && index.column() == ProcessCol::Name)
            return QStringLiteral("%1 (%2)").arg(groupLabel(g)).arg(m_groups[g].size());
        return {};
    }

    int g   = static_cast<int>(index.internalId());
    int row = index.row();
    if (g < 0 || g >= kGroupCount) return {};
    if (row < 0 || row >= m_groups[g].size()) return {};

    const ProcessInfo& pi = m_groups[g][row];

    if (role == IsGroupRole) return false;
    if (role == PidRole)     return pi.pid;
    if (role == CategoryRole) return static_cast<int>(pi.category);
    if (role == RawCpuRole)  return pi.cpuPercent;
    if (role == RawGpuRole)  return pi.gpuPercent;
    if (role == RawMemRole)  return pi.workingSetBytes;
    if (role == RawDiskRole) return pi.diskReadBytesPerSec + pi.diskWriteBytesPerSec;

    if (role == Qt::ForegroundRole && pi.status == ProcessStatus::NotResponding)
        return QBrush(QColor(Qt::red));
    if (role == Qt::ForegroundRole && pi.status == ProcessStatus::Suspended)
        return QBrush(QColor(Qt::darkYellow));

    if (role == Qt::ForegroundRole
        && (index.column() == ProcessCol::CPU || index.column() == ProcessCol::GPU)) {
        const double usage = index.column() == ProcessCol::CPU ? pi.cpuPercent : pi.gpuPercent;
        if (usage >= 75.0)
            return QBrush(QColor(0xE7, 0x4C, 0x3C));
        if (usage >= 25.0)
            return QBrush(QColor(0xF3, 0x9C, 0x12));
        return QBrush(QColor(0x2E, 0xCC, 0x71));
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return {};

    switch (index.column()) {
        case ProcessCol::Name:
            if (role == Qt::ToolTipRole && !pi.exePath.isEmpty())
                return pi.exePath;
            return pi.name;
        case ProcessCol::PID:      return pi.pid;
        case ProcessCol::Status:   return statusText(pi.status);
        case ProcessCol::CPU:      return QStringLiteral("%1%").arg(pi.cpuPercent, 0, 'f', 1);
        case ProcessCol::GPU:      return QStringLiteral("%1%").arg(pi.gpuPercent, 0, 'f', 1);
        case ProcessCol::Memory:   return formatMemory(pi.workingSetBytes);
        case ProcessCol::Disk:     return formatDisk(pi.diskReadBytesPerSec,
                                                     pi.diskWriteBytesPerSec);
        case ProcessCol::Network: {
            int total = pi.tcpConnections + pi.udpEndpoints;
            if (total == 0) return QStringLiteral("—");
            return QStringLiteral("%1 TCP / %2 UDP").arg(pi.tcpConnections).arg(pi.udpEndpoints);
        }
        case ProcessCol::Handles:  return pi.handleCount;
        case ProcessCol::Threads:  return pi.threadCount;
        case ProcessCol::Username: return pi.username;
        default: return {};
    }
}

QVariant ProcessTreeModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ProcessCol::Name:     return QStringLiteral("Name");
        case ProcessCol::PID:      return QStringLiteral("PID");
        case ProcessCol::Status:   return QStringLiteral("Status");
        case ProcessCol::CPU:      return QStringLiteral("CPU");
        case ProcessCol::GPU:      return QStringLiteral("GPU");
        case ProcessCol::Memory:   return QStringLiteral("Memory");
        case ProcessCol::Disk:     return QStringLiteral("Disk");
        case ProcessCol::Network:  return QStringLiteral("Network");
        case ProcessCol::Handles:  return QStringLiteral("Handles");
        case ProcessCol::Threads:  return QStringLiteral("Threads");
        case ProcessCol::Username: return QStringLiteral("User");
        default: return {};
    }
}

Qt::ItemFlags ProcessTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

ProcessTableModel::ProcessTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void ProcessTableModel::update(const QVector<ProcessInfo>& processes) {

    QHash<quint32, int> oldPidIdx;
    for (int i = 0; i < m_rows.size(); ++i)
        oldPidIdx.insert(m_rows[i].pid, i);

    QHash<quint32, int> newPidIdx;
    for (int i = 0; i < processes.size(); ++i)
        newPidIdx.insert(processes[i].pid, i);

    for (int i = m_rows.size() - 1; i >= 0; --i) {
        if (!newPidIdx.contains(m_rows[i].pid)) {
            beginRemoveRows({}, i, i);
            m_rows.remove(i);
            endRemoveRows();
        }
    }

    QHash<quint32, int> currentIdx;
    for (int i = 0; i < m_rows.size(); ++i)
        currentIdx.insert(m_rows[i].pid, i);

    for (int i = 0; i < m_rows.size(); ++i) {
        auto it = newPidIdx.find(m_rows[i].pid);
        if (it != newPidIdx.end()) {
            m_rows[i] = processes[it.value()];
            emit dataChanged(index(i, 0), index(i, ProcessCol::Count - 1));
        }
    }

    QVector<ProcessInfo> toAdd;
    for (const auto& pi : processes) {
        if (!currentIdx.contains(pi.pid))
            toAdd.push_back(pi);
    }
    if (!toAdd.isEmpty()) {
        int first = m_rows.size();
        int last  = first + toAdd.size() - 1;
        beginInsertRows({}, first, last);
        m_rows.append(toAdd);
        endInsertRows();
    }
}

const ProcessInfo* ProcessTableModel::infoAt(int row) const {
    if (row < 0 || row >= m_rows.size()) return nullptr;
    return &m_rows[row];
}

int ProcessTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int ProcessTableModel::columnCount(const QModelIndex&) const {
    return ProcessCol::Count;
}

QVariant ProcessTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    int r = index.row();
    if (r < 0 || r >= m_rows.size()) return {};

    const ProcessInfo& pi = m_rows[r];

    if (role == PidRole)     return pi.pid;
    if (role == RawCpuRole)  return pi.cpuPercent;
    if (role == RawGpuRole)  return pi.gpuPercent;
    if (role == RawMemRole)  return pi.workingSetBytes;
    if (role == RawDiskRole) return pi.diskReadBytesPerSec + pi.diskWriteBytesPerSec;

    if (role == Qt::ForegroundRole && pi.status == ProcessStatus::NotResponding)
        return QBrush(QColor(Qt::red));
    if (role == Qt::ForegroundRole && pi.status == ProcessStatus::Suspended)
        return QBrush(QColor(Qt::darkYellow));

    if (role == Qt::ForegroundRole
        && (index.column() == ProcessCol::CPU || index.column() == ProcessCol::GPU)) {
        const double usage = index.column() == ProcessCol::CPU ? pi.cpuPercent : pi.gpuPercent;
        if (usage >= 75.0)
            return QBrush(QColor(0xE7, 0x4C, 0x3C));
        if (usage >= 25.0)
            return QBrush(QColor(0xF3, 0x9C, 0x12));
        return QBrush(QColor(0x2E, 0xCC, 0x71));
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return {};

    switch (index.column()) {
        case ProcessCol::Name:
            if (role == Qt::ToolTipRole && !pi.exePath.isEmpty())
                return pi.exePath;
            return pi.name;
        case ProcessCol::PID:      return pi.pid;
        case ProcessCol::Status:   return fmtStatus(pi.status);
        case ProcessCol::CPU:      return QStringLiteral("%1%").arg(pi.cpuPercent, 0, 'f', 1);
        case ProcessCol::GPU:      return QStringLiteral("%1%").arg(pi.gpuPercent, 0, 'f', 1);
        case ProcessCol::Memory:   return fmtMemory(pi.workingSetBytes);
        case ProcessCol::Disk:     return fmtDisk(pi.diskReadBytesPerSec,
                                                  pi.diskWriteBytesPerSec);
        case ProcessCol::Network: {
            int total = pi.tcpConnections + pi.udpEndpoints;
            if (total == 0) return QStringLiteral("—");
            return QStringLiteral("%1 TCP / %2 UDP").arg(pi.tcpConnections).arg(pi.udpEndpoints);
        }
        case ProcessCol::Handles:  return pi.handleCount;
        case ProcessCol::Threads:  return pi.threadCount;
        case ProcessCol::Username: return pi.username;
        default: return {};
    }
}

QVariant ProcessTableModel::headerData(int section, Qt::Orientation orientation,
                                        int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ProcessCol::Name:     return QStringLiteral("Name");
        case ProcessCol::PID:      return QStringLiteral("PID");
        case ProcessCol::Status:   return QStringLiteral("Status");
        case ProcessCol::CPU:      return QStringLiteral("CPU");
        case ProcessCol::GPU:      return QStringLiteral("GPU");
        case ProcessCol::Memory:   return QStringLiteral("Memory");
        case ProcessCol::Disk:     return QStringLiteral("Disk");
        case ProcessCol::Network:  return QStringLiteral("Network");
        case ProcessCol::Handles:  return QStringLiteral("Handles");
        case ProcessCol::Threads:  return QStringLiteral("Threads");
        case ProcessCol::Username: return QStringLiteral("User");
        default: return {};
    }
}

Qt::ItemFlags ProcessTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QString ProcessTableModel::formatMemory(quint64 b) { return fmtMemory(b); }
QString ProcessTableModel::formatDisk(quint64 r, quint64 w) { return fmtDisk(r, w); }
QString ProcessTableModel::statusText(ProcessStatus s) { return fmtStatus(s); }
QString ProcessTableModel::priorityText(int cls) { return fmtPriority(cls); }

ProcessFilterProxy::ProcessFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void ProcessFilterProxy::setFilterText(const QString& text) {
    m_filterText = text;
    invalidateFilter();
}

bool ProcessFilterProxy::filterAcceptsRow(int sourceRow,
                                           const QModelIndex&) const {
    if (m_filterText.isEmpty()) return true;
    auto* src = qobject_cast<ProcessTableModel*>(sourceModel());
    if (!src) return true;
    const ProcessInfo* pi = src->infoAt(sourceRow);
    if (!pi) return false;
    return pi->name.contains(m_filterText, Qt::CaseInsensitive)
        || pi->exePath.contains(m_filterText, Qt::CaseInsensitive)
        || pi->username.contains(m_filterText, Qt::CaseInsensitive)
        || QString::number(pi->pid).contains(m_filterText);
}

bool ProcessFilterProxy::lessThan(const QModelIndex& left,
                                   const QModelIndex& right) const {

    int col = left.column();
    if (col == ProcessCol::CPU) {
        return left.data(RawCpuRole).toDouble()
             < right.data(RawCpuRole).toDouble();
    }
    if (col == ProcessCol::GPU) {
        return left.data(RawGpuRole).toDouble()
             < right.data(RawGpuRole).toDouble();
    }
    if (col == ProcessCol::Memory) {
        return left.data(RawMemRole).toULongLong()
             < right.data(RawMemRole).toULongLong();
    }
    if (col == ProcessCol::Disk) {
        return left.data(RawDiskRole).toULongLong()
             < right.data(RawDiskRole).toULongLong();
    }
    if (col == ProcessCol::PID || col == ProcessCol::Handles ||
        col == ProcessCol::Threads) {
        return left.data(Qt::DisplayRole).toUInt()
             < right.data(Qt::DisplayRole).toUInt();
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

}
