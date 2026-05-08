#pragma once

#include "Candle.h"

#include <QChartView>
#include <QString>
#include <QVector>

// Owns a QChart that renders a candlestick series plus optional indicator overlays.
// Build it once, then call setData() / addOverlay() whenever the data changes.
class CandleChart : public QChartView {
    Q_OBJECT
public:
    explicit CandleChart(QWidget* parent = nullptr);

    void setData(const QString& symbol, const CandleSeries& candles);

    // Adds a line overlay (e.g. SMA20) on top of the price series.
    // values must be the same length as the last setData() candles; NaN entries are skipped.
    void addOverlay(const QString& name, const QVector<double>& values);
    void clearOverlays();

private:
    void rebuildAxes();

    CandleSeries m_lastCandles;
    QString      m_symbol;
};
