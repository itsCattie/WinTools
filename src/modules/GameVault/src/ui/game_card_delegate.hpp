#pragma once

// GameVault: game card delegate manages UI behavior and presentation.

#include <QAbstractItemDelegate>
#include <QHash>
#include <QPixmap>
#include <QString>

namespace wintools::gamevault {

class GameCardDelegate : public QAbstractItemDelegate {
    Q_OBJECT
public:

    static constexpr int kCardW     = 210;
    static constexpr int kCardH     = 155;
    static constexpr int kArtH      = 99;
    static constexpr int kPadding   = 8;
    static constexpr int kRadius    = 6;

    explicit GameCardDelegate(QObject* parent = nullptr);

    void setArtCache(const QHash<QString, QPixmap>* cache) { m_artCache = cache; }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex&           index) const override;

    void paint(QPainter*                   painter,
               const QStyleOptionViewItem& option,
               const QModelIndex&          index) const override;

private:
    const QHash<QString, QPixmap>* m_artCache = nullptr;
    mutable QHash<QString, QPixmap> m_scaledArtCache;
    mutable QHash<QString, qint64>  m_scaledArtSourceKeys;
    mutable QSize                   m_scaledArtSize;
};

}
