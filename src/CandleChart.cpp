#include "CandleChart.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QDateTimeAxis>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QLineSeries>
#include <QLocale>
#include <QMouseEvent>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

CandleChart::CandleChart(QWidget* parent)
    : QChartView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    chart()->setAnimationOptions(QChart::NoAnimation);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    m_crosshairLine = new QGraphicsLineItem();
    m_crosshairLine->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_crosshairLine->setZValue(10);
    m_crosshairLine->setVisible(false);
    scene()->addItem(m_crosshairLine);

    m_infoBg = new QGraphicsRectItem();
    m_infoBg->setBrush(QBrush(QColor(20, 20, 20, 210)));
    m_infoBg->setPen(Qt::NoPen);
    m_infoBg->setZValue(11);
    m_infoBg->setVisible(false);
    scene()->addItem(m_infoBg);

    m_infoText = new QGraphicsTextItem();
    m_infoText->setDefaultTextColor(Qt::white);
    m_infoText->setFont(QFont("Consolas", 8));
    m_infoText->setZValue(12);
    m_infoText->setVisible(false);
    scene()->addItem(m_infoText);
}

void CandleChart::setData(const QString& symbol, const CandleSeries& candles)
{
    m_symbol      = symbol;
    m_lastCandles = candles;
    hideCrosshair();

    QChart* c = chart();
    c->removeAllSeries();
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

// ── mouse events ─────────────────────────────────────────────────────────────

void CandleChart::mouseMoveEvent(QMouseEvent* event)
{
    QChartView::mouseMoveEvent(event);
    if (m_lastCandles.isEmpty()) return;

    const QPointF sp = mapToScene(event->pos());
    const QRectF  pa = chart()->plotArea();

    if (!pa.contains(sp)) { hideCrosshair(); return; }

    const qint64 tsMs = static_cast<qint64>(chart()->mapToValue(sp).x());
    paintCrosshairAt(sp.x(), tsMs);
    emit crosshairMoved(tsMs);
}

void CandleChart::leaveEvent(QEvent* event)
{
    QChartView::leaveEvent(event);
    hideCrosshair();
    emit crosshairLeft();
}

// ── public slots ──────────────────────────────────────────────────────────────

void CandleChart::updateCrosshair(qint64 timestampMs)
{
    if (m_lastCandles.isEmpty()) return;
    const double sx = chart()->mapToPosition(QPointF(timestampMs, 0)).x();
    const QRectF  pa = chart()->plotArea();
    if (sx >= pa.left() && sx <= pa.right())
        paintCrosshairAt(sx, timestampMs);
}

void CandleChart::hideCrosshair()
{
    m_crosshairLine->setVisible(false);
    m_infoBg->setVisible(false);
    m_infoText->setVisible(false);
}

// ── private helpers ───────────────────────────────────────────────────────────

void CandleChart::paintCrosshairAt(double sceneX, qint64 timestampMs)
{
    const QRectF pa = chart()->plotArea();

    m_crosshairLine->setLine(sceneX, pa.top(), sceneX, pa.bottom());
    m_crosshairLine->setVisible(true);

    const int idx = nearestCandle(timestampMs);
    if (idx < 0) return;

    const Candle& bar = m_lastCandles[idx];
    const QString text = QString(
        "%1\n"
        "O: %2   H: %3\n"
        "C: %5   L: %4\n"
        "V: %6"
    ).arg(bar.timestamp.toString("yyyy-MM-dd"))
     .arg(bar.open,  0, 'f', 2)
     .arg(bar.high,  0, 'f', 2)
     .arg(bar.low,   0, 'f', 2)
     .arg(bar.close, 0, 'f', 2)
     .arg(QLocale().toString(bar.volume));

    m_infoText->setPlainText(text);

    constexpr qreal margin = 8;
    const QRectF tb = m_infoText->boundingRect();
    m_infoBg->setRect(pa.left() + margin - 3,
                      pa.top()  + margin - 3,
                      tb.width()  + 6,
                      tb.height() + 6);
    m_infoText->setPos(pa.left() + margin, pa.top() + margin);

    m_infoBg->setVisible(true);
    m_infoText->setVisible(true);
}

int CandleChart::nearestCandle(qint64 tsMs) const
{
    if (m_lastCandles.isEmpty()) return -1;
    int lo = 0, hi = m_lastCandles.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (m_lastCandles[mid].timestamp.toMSecsSinceEpoch() < tsMs)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo > 0) {
        const qint64 d0 = qAbs(m_lastCandles[lo-1].timestamp.toMSecsSinceEpoch() - tsMs);
        const qint64 d1 = qAbs(m_lastCandles[lo].timestamp.toMSecsSinceEpoch()   - tsMs);
        if (d0 < d1) --lo;
    }
    return lo;
}
