#pragma once

// StreamVault: stream card delegate manages UI behavior and presentation.

#include <QAbstractItemDelegate>
#include <QHash>
#include <QPixmap>
#include <QString>

namespace wintools::streamvault {

class StreamCardDelegate : public QAbstractItemDelegate {
    Q_OBJECT
public:

    static constexpr int kCardW   = 160;
    static constexpr int kCardH   = 290;
    static constexpr int kArtH    = 225;
    static constexpr int kPadding = 7;
    static constexpr int kRadius  = 6;

    explicit StreamCardDelegate(QObject* parent = nullptr);

    void setPosterCache(const QHash<int, QPixmap>* cache) { m_posterCache = cache; }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex&           index) const override;

    void paint(QPainter*                   painter,
               const QStyleOptionViewItem& option,
               const QModelIndex&          index) const override;

private:
    const QHash<int, QPixmap>* m_posterCache = nullptr;

    static void drawPlaceholder(QPainter* p, const QRect& artRect, const QString& title);
};

}
