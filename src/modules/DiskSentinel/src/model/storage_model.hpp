#pragma once

#include "modules/disksentinel/src/core/disk_node.hpp"

#include <QAbstractItemModel>
#include <QHash>
#include <QIcon>
#include <memory>

namespace wintools::disksentinel {

class StorageModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column { ColName = 0, ColSize, ColShare, ColItems, ColCount };

    static constexpr int RawSizeRole  = Qt::UserRole + 1;
    static constexpr int FractionRole = Qt::UserRole + 2;
    static constexpr int IsDirRole    = Qt::UserRole + 3;

    explicit StorageModel(QObject* parent = nullptr);

    void setRoot(std::shared_ptr<DiskNode> root);
    DiskNode* root() const { return m_root.get(); }

    QModelIndex   index(int row, int column,
                        const QModelIndex& parent = {}) const override;
    QModelIndex   parent(const QModelIndex& child)      const override;
    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void sort(int column, Qt::SortOrder order) override;

    static DiskNode* nodeForIndex(const QModelIndex& index);
    QModelIndex      indexForNode(DiskNode* node) const;

private:
    void ensureIconsInitialized() const;
    void sortNode(DiskNode* node, int column, Qt::SortOrder order);
    void invalidateDisplayCache();

    std::shared_ptr<DiskNode> m_root;
    int           m_sortColumn = ColSize;
    Qt::SortOrder m_sortOrder  = Qt::DescendingOrder;

    mutable QHash<DiskNode*, QString> m_sizeCache;
    mutable QIcon m_dirIcon;
    mutable QIcon m_fileIcon;
    mutable bool  m_iconsInitialized = false;
};

}
