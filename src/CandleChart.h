#pragma once

#include "Candle.h"

#include <QChartView>
#include <QString>
#include <QVector>

class QDateTimeAxis;
class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsTextItem;
class QValueAxis;

class CandleChart : public QChartView {
    Q_OBJECT
public:
    explicit CandleChart(QWidget* parent = nullptr);

    void setData(const QString& symbol, const CandleSeries& candles);
    void addOverlay(const QString& name, const QVector<double>& values);
    void clearOverlays();

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
    QString      m_symbol;

    // Axes kept as members so addOverlay() can attach only to price axes.
    QDateTimeAxis* m_xAxis     = nullptr;
    QValueAxis*    m_priceAxis = nullptr;
    QValueAxis*    m_volAxis   = nullptr;

    QGraphicsLineItem* m_crosshairLine = nullptr;
    QGraphicsRectItem* m_infoBg        = nullptr;
    QGraphicsTextItem* m_infoText      = nullptr;
};
