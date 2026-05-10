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
#include <QPen>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

static constexpr double kVolScale = 1e6; // display in millions

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
    m_xAxis       = nullptr;
    m_priceAxis   = nullptr;
    m_volAxis     = nullptr;
    hideCrosshair();

    QChart* c = chart();
    c->removeAllSeries();
    for (QAbstractAxis* a : c->axes()) c->removeAxis(a);
    c->legend()->hide();

    if (candles.isEmpty()) return;

    // ── Candle series ─────────────────────────────────────────────────────────
    auto* candleSeries = new QCandlestickSeries();
    candleSeries->setName(symbol);
    candleSeries->setIncreasingColor(QColor(0, 160, 80));
    candleSeries->setDecreasingColor(QColor(200, 50, 50));
    for (const Candle& bar : candles) {
        candleSeries->append(new QCandlestickSet(
            bar.open, bar.high, bar.low, bar.close,
            bar.timestamp.toMSecsSinceEpoch()));
    }

    // ── Volume series (bull / bear, semi-transparent) ─────────────────────────
    double maxVol = 0.0;
    for (const Candle& bar : candles)
        maxVol = std::max(maxVol, static_cast<double>(bar.volume));
    const bool hasVolume = maxVol > 0.0;

    auto makeVolSeries = [](QColor color) {
        auto* s = new QCandlestickSeries();
        s->setBodyWidth(0.7);
        s->setIncreasingColor(color);
        s->setDecreasingColor(color);
        QPen p(color); p.setWidthF(0.3);
        s->setPen(p);
        return s;
    };

    QCandlestickSeries* bullVol = nullptr;
    QCandlestickSeries* bearVol = nullptr;
    if (hasVolume) {
        bullVol = makeVolSeries(QColor(0,   160,  80, 70));
        bearVol = makeVolSeries(QColor(200,  50,  50, 70));
        for (const Candle& bar : candles) {
            const double vol = bar.volume / kVolScale;
            const qint64 ts  = bar.timestamp.toMSecsSinceEpoch();
            if (bar.close >= bar.open)
                bullVol->append(new QCandlestickSet(0, vol, 0, vol, ts));
            else
                bearVol->append(new QCandlestickSet(vol, vol, 0, 0, ts));
        }
    }

    // ── Axes ──────────────────────────────────────────────────────────────────
    m_xAxis = new QDateTimeAxis();
    m_xAxis->setFormat("yyyy-MM-dd");
    m_xAxis->setTitleText("Date");
    m_xAxis->setRange(candles.first().timestamp, candles.last().timestamp);
    c->addAxis(m_xAxis, Qt::AlignBottom);

    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (const Candle& b : candles) { lo = std::min(lo, b.low); hi = std::max(hi, b.high); }
    const double pad = (hi - lo) * 0.05;

    m_priceAxis = new QValueAxis();
    m_priceAxis->setTitleText("Price");
    m_priceAxis->setRange(lo - pad, hi + pad);
    c->addAxis(m_priceAxis, Qt::AlignLeft);

    // Add series to chart and attach to price axes.
    c->addSeries(candleSeries);
    candleSeries->attachAxis(m_xAxis);
    candleSeries->attachAxis(m_priceAxis);

    if (hasVolume) {
        // Volume axis on the right: range 0 → maxVol×4 keeps bars in bottom 25%.
        m_volAxis = new QValueAxis();
        m_volAxis->setTitleText("Vol (M)");
        m_volAxis->setLabelFormat("%.0f");
        m_volAxis->setTickCount(3);
        m_volAxis->setRange(0, (maxVol / kVolScale) * 4.0);
        c->addAxis(m_volAxis, Qt::AlignRight);

        c->addSeries(bullVol);
        c->addSeries(bearVol);
        bullVol->attachAxis(m_xAxis);
        bullVol->attachAxis(m_volAxis);
        bearVol->attachAxis(m_xAxis);
        bearVol->attachAxis(m_volAxis);
    }

    c->setTitle(symbol);
}

void CandleChart::addOverlay(const QString& name, const QVector<double>& values)
{
    if (values.size() != m_lastCandles.size() || !m_xAxis || !m_priceAxis) return;

    auto* line = new QLineSeries();
    line->setName(name);
    for (int i = 0; i < values.size(); ++i) {
        if (std::isnan(values[i])) continue;
        line->append(m_lastCandles[i].timestamp.toMSecsSinceEpoch(), values[i]);
    }

    chart()->addSeries(line);
    // Attach only to price axes — NOT to the volume right axis.
    line->attachAxis(m_xAxis);
    line->attachAxis(m_priceAxis);
}

void CandleChart::clearOverlays()
{
    QChart* c = chart();
    const auto seriesList = c->series();
    for (QAbstractSeries* s : seriesList) {
        if (qobject_cast<QLineSeries*>(s)) c->removeSeries(s);
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
