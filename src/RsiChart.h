#pragma once

#include <QChartView>
#include <QDateTime>
#include <QVector>

class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsTextItem;

class RsiChart : public QChartView {
    Q_OBJECT
public:
    explicit RsiChart(QWidget* parent = nullptr);

    void setData(const QVector<QDateTime>& timestamps, const QVector<double>& values);
    void clear();

signals:
    void crosshairMoved(qint64 timestampMs);
    void crosshairLeft();

public slots:
    void updateCrosshair(qint64 timestampMs);
    void hideCrosshair();

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void paintCrosshairAt(double sceneX, qint64 timestampMs);
    int  nearestSample(qint64 timestampMs) const;

    QVector<QDateTime> m_timestamps;
    QVector<double>    m_values;

    QGraphicsLineItem* m_crosshairLine = nullptr;
    QGraphicsRectItem* m_infoBg        = nullptr;
    QGraphicsTextItem* m_infoText      = nullptr;
};
