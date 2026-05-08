#include "CandleChart.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QDateTimeAxis>
#include <QLineSeries>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

CandleChart::CandleChart(QWidget* parent)
    : QChartView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    chart()->setAnimationOptions(QChart::NoAnimation);
}

void CandleChart::setData(const QString& symbol, const CandleSeries& candles)
{
    m_symbol      = symbol;
    m_lastCandles = candles;

    QChart* c = chart();
    c->removeAllSeries();
    // Detach any axes left over from a prior setData() call.
    const auto axes = c->axes();
    for (QAbstractAxis* a : axes) c->removeAxis(a);

    auto* series = new QCandlestickSeries();
    series->setName(symbol);
    series->setIncreasingColor(QColor(0, 160, 80));
    series->setDecreasingColor(QColor(200, 50, 50));

    for (const Candle& bar : candles) {
        auto* set = new QCandlestickSet(
            bar.open, bar.high, bar.low, bar.close,
            bar.timestamp.toMSecsSinceEpoch());
        series->append(set);
    }

    c->addSeries(series);
    c->setTitle(symbol);

    rebuildAxes();
}

void CandleChart::addOverlay(const QString& name, const QVector<double>& values)
{
    if (values.size() != m_lastCandles.size()) return;

    auto* line = new QLineSeries();
    line->setName(name);
    for (int i = 0; i < values.size(); ++i) {
        if (std::isnan(values[i])) continue;
        line->append(m_lastCandles[i].timestamp.toMSecsSinceEpoch(), values[i]);
    }

    QChart* c = chart();
    c->addSeries(line);
    // Attach the new series to whatever axes already exist.
    for (QAbstractAxis* a : c->axes()) line->attachAxis(a);
}

void CandleChart::clearOverlays()
{
    QChart* c = chart();
    const auto seriesList = c->series();
    for (QAbstractSeries* s : seriesList) {
        if (qobject_cast<QLineSeries*>(s)) c->removeSeries(s);
    }
}

void CandleChart::rebuildAxes()
{
    if (m_lastCandles.isEmpty()) return;

    QChart* c = chart();

    auto* xAxis = new QDateTimeAxis();
    xAxis->setFormat("yyyy-MM-dd");
    xAxis->setTitleText("Date");
    xAxis->setRange(m_lastCandles.first().timestamp,
                    m_lastCandles.last().timestamp);
    c->addAxis(xAxis, Qt::AlignBottom);

    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (const Candle& b : m_lastCandles) {
        lo = std::min(lo, b.low);
        hi = std::max(hi, b.high);
    }
    const double pad = (hi - lo) * 0.05;

    auto* yAxis = new QValueAxis();
    yAxis->setTitleText("Price");
    yAxis->setRange(lo - pad, hi + pad);
    c->addAxis(yAxis, Qt::AlignLeft);

    for (QAbstractSeries* s : c->series()) {
        s->attachAxis(xAxis);
        s->attachAxis(yAxis);
    }
}
