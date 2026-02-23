#pragma once

// DiskSentinel: treemap widget manages UI behavior and presentation.

#include "modules/disksentinel/src/core/disk_node.hpp"

#include <QWidget>
#include <QVector>
#include <QPair>
#include <QRectF>

namespace wintools::disksentinel {

class TreemapWidget : public QWidget {
    Q_OBJECT

public:
    explicit TreemapWidget(QWidget* parent = nullptr);

    void setRoot(DiskNode* root);
    DiskNode* currentRoot() const { return m_root; }

    QSize sizeHint() const override { return {600, 400}; }

signals:
    void nodeClicked(wintools::disksentinel::DiskNode* node);
    void nodeHovered(wintools::disksentinel::DiskNode* node);

protected:
    void paintEvent(QPaintEvent* event)     override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event)override;
    void leaveEvent(QEvent* event)          override;
    void resizeEvent(QResizeEvent* event)   override;

private:

    struct CellRecord {
        QRectF    rect;
        DiskNode* node = nullptr;
    };

    void   renderRoot(QPainter& p);
    void   layoutItems(QPainter& p, QRectF rect,
                       const QVector<DiskNode*>& items,
                       qint64 total, int depth);
    void   drawNode(QPainter& p, QRectF rect, DiskNode* node, int depth);

    static QColor colorForNode(DiskNode* node, int depth);
    static QColor lightenForDepth(QColor base, int depth);

    DiskNode* hitTest(const QPointF& pos) const;

    DiskNode*           m_root    = nullptr;
    DiskNode*           m_hovered = nullptr;
    QVector<CellRecord> m_cells;
};

}
