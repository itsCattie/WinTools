#include "modules/disksentinel/src/model/storage_model.hpp"

#include "logger/logger.hpp"

#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <algorithm>
#include <vector>

namespace wintools::disksentinel {

static constexpr const char* kLog = "DiskSentinel/Model";

StorageModel::StorageModel(QObject* parent)
    : QAbstractItemModel(parent) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                   "StorageModel created.");
}

void StorageModel::ensureIconsInitialized() const {
    if (m_iconsInitialized) return;

    if (qApp && qApp->style()) {
        m_dirIcon  = qApp->style()->standardIcon(QStyle::SP_DirIcon);
        m_fileIcon = qApp->style()->standardIcon(QStyle::SP_FileIcon);
    }
    if (m_dirIcon.isNull()) {
        m_dirIcon = QIcon::fromTheme(QStringLiteral("folder"),
                                     QIcon(QStringLiteral(":/icons/folder")));
    }
    if (m_fileIcon.isNull()) {
        m_fileIcon = QIcon::fromTheme(QStringLiteral("text-x-generic"),
                                      QIcon(QStringLiteral(":/icons/file")));
    }

    m_iconsInitialized = true;
}

void StorageModel::setRoot(std::shared_ptr<DiskNode> root) {
    if (root) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("setRoot: '%1', %2 direct children, %3 total.")
                .arg(root->path)
                .arg(root->children.size())
                .arg(DiskNode::prettySize(root->size)));
    } else {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                       "setRoot: clearing model (null root).");
    }
    beginResetModel();
    m_root = std::move(root);
    invalidateDisplayCache();
    endResetModel();
}

DiskNode* StorageModel::nodeForIndex(const QModelIndex& index) {
    if (!index.isValid()) return nullptr;
    return static_cast<DiskNode*>(index.internalPointer());
}

QModelIndex StorageModel::indexForNode(DiskNode* node) const {
    if (!node || !node->parent) return {};
    DiskNode* par = node->parent;
    for (int i = 0; i < static_cast<int>(par->children.size()); ++i) {
        if (par->children[i].get() == node)
            return createIndex(i, 0, node);
    }
    return {};
}

QModelIndex StorageModel::index(int row, int column,
                                 const QModelIndex& parent) const {
    if (!m_root) return {};

    DiskNode* parentNode = parent.isValid()
        ? static_cast<DiskNode*>(parent.internalPointer())
        : m_root.get();

    if (row < 0 || row >= static_cast<int>(parentNode->children.size()))
        return {};

    return createIndex(row, column, parentNode->children[row].get());
}

QModelIndex StorageModel::parent(const QModelIndex& child) const {
    if (!child.isValid() || !m_root) return {};

    DiskNode* node       = static_cast<DiskNode*>(child.internalPointer());
    DiskNode* parentNode = node->parent;

    if (!parentNode || parentNode == m_root.get()) return {};

    DiskNode* grandParent = parentNode->parent;
    if (!grandParent) return {};

    for (int i = 0; i < static_cast<int>(grandParent->children.size()); ++i) {
        if (grandParent->children[i].get() == parentNode)
            return createIndex(i, 0, parentNode);
    }
    return {};
}

int StorageModel::rowCount(const QModelIndex& parent) const {
    if (!m_root) return 0;
    DiskNode* node = parent.isValid()
        ? static_cast<DiskNode*>(parent.internalPointer())
        : m_root.get();
    return static_cast<int>(node->children.size());
}

int StorageModel::columnCount(const QModelIndex&) const {
    return ColCount;
}

QVariant StorageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    auto* node = static_cast<DiskNode*>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColName:  return node->name;
        case ColSize: {

            auto it = m_sizeCache.constFind(node);
            if (it != m_sizeCache.constEnd()) return it.value();
            const QString s = DiskNode::prettySize(node->size);
            m_sizeCache.insert(node, s);
            return s;
        }
        case ColShare: {
            const double f = node->fractionOfParent() * 100.0;
            return QString::number(f, 'f', 1) + QStringLiteral(" %");
        }
        case ColItems:
            return node->isDir ? (node->itemCount > 0
                                  ? QString::number(node->itemCount)
                                  : QStringLiteral("—"))
                               : QStringLiteral("—");
        }
        return {};
    }

    if (role == Qt::DecorationRole && index.column() == ColName) {
        ensureIconsInitialized();
        return node->isDir ? m_dirIcon : m_fileIcon;
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColSize || index.column() == ColItems)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        if (index.column() == ColShare)
            return QVariant(Qt::AlignCenter);
    }

    if (role == RawSizeRole)  return node->size;
    if (role == FractionRole) return node->fractionOfParent();
    if (role == IsDirRole)    return node->isDir;

    if (role == Qt::ToolTipRole)
        return QStringLiteral("%1\n%2")
            .arg(node->path, DiskNode::prettySize(node->size));

    return {};
}

QVariant StorageModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColName:  return QStringLiteral("Name");
    case ColSize:  return QStringLiteral("Size");
    case ColShare: return QStringLiteral("% of Parent");
    case ColItems: return QStringLiteral("Items");
    }
    return {};
}

Qt::ItemFlags StorageModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void StorageModel::sort(int column, Qt::SortOrder order) {
    if (!m_root) return;

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("sort: column=%1 order=%2")
            .arg(column)
            .arg(order == Qt::AscendingOrder ? "Asc" : "Desc"));

    m_sortColumn = column;
    m_sortOrder  = order;

    const QModelIndexList persistent = persistentIndexList();
    QVector<DiskNode*> persistentNodes;
    persistentNodes.reserve(persistent.size());
    for (const QModelIndex& idx : persistent)
        persistentNodes.push_back(nodeForIndex(idx));

    emit layoutAboutToBeChanged();
    sortNode(m_root.get(), column, order);

    QModelIndexList newIndexes;
    newIndexes.reserve(persistent.size());
    for (DiskNode* node : persistentNodes)
        newIndexes.push_back(node ? indexForNode(node) : QModelIndex{});
    changePersistentIndexList(persistent, newIndexes);

    invalidateDisplayCache();
    emit layoutChanged();
}

void StorageModel::invalidateDisplayCache() {
    m_sizeCache.clear();
}

void StorageModel::sortNode(DiskNode* node, int column, Qt::SortOrder order) {
    if (!node) return;

    auto cmp = [column, order](const std::shared_ptr<DiskNode>& a,
                               const std::shared_ptr<DiskNode>& b) -> bool {
        if (!a || !b) return !a && b;

        if (a->isDir != b->isDir) return a->isDir;

        const bool asc = (order == Qt::AscendingOrder);
        switch (column) {
        case ColName: {
            const auto la = a->name.toLower();
            const auto lb = b->name.toLower();
            return asc ? (la < lb) : (la > lb);
        }
        case ColShare:
        case ColSize:
            return asc ? (a->size < b->size) : (a->size > b->size);
        case ColItems:
            return asc ? (a->itemCount < b->itemCount) : (a->itemCount > b->itemCount);
        default:
            return asc ? (a->size < b->size) : (a->size > b->size);
        }
    };

    std::vector<DiskNode*> stack;
    stack.push_back(node);
    while (!stack.empty()) {
        DiskNode* current = stack.back();
        stack.pop_back();
        if (!current || current->children.empty()) continue;
        std::sort(current->children.begin(), current->children.end(), cmp);
        for (auto& child : current->children)
            if (child && child->isDir)
                stack.push_back(child.get());
    }
}

}
