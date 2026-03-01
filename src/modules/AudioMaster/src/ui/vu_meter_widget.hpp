#pragma once

#include <QWidget>

namespace wintools::audiomaster {

class VuMeterWidget : public QWidget {
    Q_OBJECT
public:
    explicit VuMeterWidget(QWidget* parent = nullptr);

    void setLevel(float level);
    float level() const { return m_level; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    float m_level{0.0f};
};

}
