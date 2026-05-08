#pragma once

#include "Candle.h"

#include <QChartView>

class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsTextItem;

class VolumeChart : public QChartView {
    Q_OBJECT
public:
    explicit VolumeChart(QWidget* parent = nullptr);

    void setData(const CandleSeries& candles);
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
    int  nearestCandle(qint64 timestampMs) const;

    CandleSeries m_lastCandles;

    QGraphicsLineItem* m_crosshairLine = nullptr;
    QGraphicsRectItem* m_infoBg        = nullptr;
    QGraphicsTextItem* m_infoText      = nullptr;
};
