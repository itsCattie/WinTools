#pragma once

#include "modules/disksentinel/src/core/disk_node.hpp"
#include "common/themes/window_colour.hpp"

#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

namespace wintools::disksentinel {

class PieChartWidget : public QWidget {
    Q_OBJECT

public:
    explicit PieChartWidget(QWidget* parent = nullptr);

    void setRoot(DiskNode* root);
    void setThemePalette(const wintools::themes::ThemePalette& palette);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Slice {
        QString category;
        qint64  bytes = 0;
        double  fraction = 0.0;
        QColor  color;
    };

    void rebuildSlices();
    static QColor colorForCategory(const QString& cat, bool isDark);

    DiskNode* m_root = nullptr;
    QVector<Slice> m_slices;
    wintools::themes::ThemePalette m_palette;
};

}
