#include "VolumeChart.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QDateTimeAxis>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QLocale>
#include <QMouseEvent>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <limits>

static constexpr double kVolScale = 1e6; // display volumes in millions

VolumeChart::VolumeChart(QWidget* parent)
    : QChartView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    chart()->setAnimationOptions(QChart::NoAnimation);
    chart()->setTitle("Volume");
    chart()->legend()->hide();
    setFixedHeight(240);
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

void VolumeChart::clear()
{
    m_lastCandles.clear();
    hideCrosshair();

    QChart* c = chart();
    c->removeAllSeries();
    const auto axes = c->axes();
    for (QAbstractAxis* a : axes) c->removeAxis(a);
}

void VolumeChart::setData(const CandleSeries& candles)
{
    clear();
    m_lastCandles = candles;
    if (candles.isEmpty()) return;

    // Two series so each can have its own solid color (bull=green, bear=red).
    auto makeSeries = [](QColor color) {
        auto* s = new QCandlestickSeries();
        s->setBodyWidth(0.8);
        s->setIncreasingColor(color);
        s->setDecreasingColor(color);
        QPen p(color);
        p.setWidthF(0.5);
        s->setPen(p);
        return s;
    };

    auto* bullSeries = makeSeries(QColor(0, 160, 80, 200));
    auto* bearSeries = makeSeries(QColor(200, 50, 50, 200));

    double maxVol = 0;
    for (const Candle& bar : candles) {
        const double vol = bar.volume / kVolScale;
        maxVol = std::max(maxVol, vol);
        const qint64 ts = bar.timestamp.toMSecsSinceEpoch();

        // Body from 0→vol (bull) or vol→0 (bear) so Qt Charts picks the right color.
        if (bar.close >= bar.open)
            bullSeries->append(new QCandlestickSet(0, vol, 0, vol, ts));
        else
            bearSeries->append(new QCandlestickSet(vol, vol, 0, 0, ts));
    }

    QChart* c = chart();
    c->addSeries(bullSeries);
    c->addSeries(bearSeries);

    auto* xAxis = new QDateTimeAxis();
    xAxis->setFormat("MMM yy");
    xAxis->setRange(candles.first().timestamp, candles.last().timestamp);
    c->addAxis(xAxis, Qt::AlignBottom);

    auto* yAxis = new QValueAxis();
    yAxis->setRange(0, maxVol * 1.15);
    yAxis->setTickCount(3);
    yAxis->setTitleText("Vol (M)");
    yAxis->setLabelFormat("%.0f");
    c->addAxis(yAxis, Qt::AlignLeft);

    for (QAbstractSeries* s : c->series()) {
        s->attachAxis(xAxis);
        s->attachAxis(yAxis);
    }
}

// ── mouse events ─────────────────────────────────────────────────────────────

void VolumeChart::mouseMoveEvent(QMouseEvent* event)
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

void VolumeChart::leaveEvent(QEvent* event)
{
    QChartView::leaveEvent(event);
    hideCrosshair();
    emit crosshairLeft();
}

// ── public slots ──────────────────────────────────────────────────────────────

void VolumeChart::updateCrosshair(qint64 timestampMs)
{
    if (m_lastCandles.isEmpty()) return;
    const double sx = chart()->mapToPosition(QPointF(timestampMs, 0)).x();
    const QRectF  pa = chart()->plotArea();
    if (sx >= pa.left() && sx <= pa.right())
        paintCrosshairAt(sx, timestampMs);
}

void VolumeChart::hideCrosshair()
{
    m_crosshairLine->setVisible(false);
    m_infoBg->setVisible(false);
    m_infoText->setVisible(false);
}

// ── private helpers ───────────────────────────────────────────────────────────

void VolumeChart::paintCrosshairAt(double sceneX, qint64 timestampMs)
{
    const QRectF pa = chart()->plotArea();

    m_crosshairLine->setLine(sceneX, pa.top(), sceneX, pa.bottom());
    m_crosshairLine->setVisible(true);

    const int idx = nearestCandle(timestampMs);
    if (idx < 0) return;

    const Candle& bar = m_lastCandles[idx];
    m_infoText->setPlainText(
        QString("V: %1").arg(QLocale().toString(bar.volume)));

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

int VolumeChart::nearestCandle(qint64 tsMs) const
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
