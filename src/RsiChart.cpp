#include "RsiChart.h"

#include <QChart>
#include <QDateTimeAxis>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QLineSeries>
#include <QMouseEvent>
#include <QValueAxis>

#include <cmath>

RsiChart::RsiChart(QWidget* parent)
    : QChartView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    chart()->setAnimationOptions(QChart::NoAnimation);
    chart()->setTitle("RSI");
    chart()->legend()->hide();
    setFixedHeight(240);
    setMouseTracking(true);

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

void RsiChart::clear()
{
    m_timestamps.clear();
    m_values.clear();
    hideCrosshair();

    QChart* c = chart();
    c->removeAllSeries();
    const auto axes = c->axes();
    for (QAbstractAxis* a : axes) c->removeAxis(a);
}

void RsiChart::setData(const QVector<QDateTime>& timestamps, const QVector<double>& values)
{
    clear();
    m_timestamps = timestamps;
    m_values     = values;

    if (timestamps.isEmpty() || values.isEmpty()) return;

    auto* rsiLine = new QLineSeries();
    rsiLine->setColor(QColor(100, 150, 255));

    for (int i = 0; i < values.size(); ++i) {
        if (std::isnan(values[i])) continue;
        rsiLine->append(timestamps[i].toMSecsSinceEpoch(), values[i]);
    }

    const qint64 t0 = timestamps.first().toMSecsSinceEpoch();
    const qint64 t1 = timestamps.last().toMSecsSinceEpoch();

    auto* obLine = new QLineSeries();
    QPen obPen(QColor(200, 50, 50));
    obPen.setStyle(Qt::DashLine);
    obLine->setPen(obPen);
    obLine->append(t0, 70);
    obLine->append(t1, 70);

    auto* osLine = new QLineSeries();
    QPen osPen(QColor(0, 160, 80));
    osPen.setStyle(Qt::DashLine);
    osLine->setPen(osPen);
    osLine->append(t0, 30);
    osLine->append(t1, 30);

    QChart* c = chart();
    c->addSeries(rsiLine);
    c->addSeries(obLine);
    c->addSeries(osLine);

    auto* xAxis = new QDateTimeAxis();
    xAxis->setFormat("MMM yy");
    xAxis->setRange(timestamps.first(), timestamps.last());
    c->addAxis(xAxis, Qt::AlignBottom);

    auto* yAxis = new QValueAxis();
    yAxis->setRange(0, 100);
    yAxis->setTickCount(5);
    yAxis->setTitleText("RSI");
    c->addAxis(yAxis, Qt::AlignLeft);

    for (QAbstractSeries* s : c->series()) {
        s->attachAxis(xAxis);
        s->attachAxis(yAxis);
    }
}

// ── mouse events ─────────────────────────────────────────────────────────────

void RsiChart::mouseMoveEvent(QMouseEvent* event)
{
    QChartView::mouseMoveEvent(event);
    if (m_timestamps.isEmpty()) return;

    const QPointF sp = mapToScene(event->pos());
    const QRectF  pa = chart()->plotArea();

    if (!pa.contains(sp)) { hideCrosshair(); return; }

    const qint64 tsMs = static_cast<qint64>(chart()->mapToValue(sp).x());
    paintCrosshairAt(sp.x(), tsMs);
    emit crosshairMoved(tsMs);
}

void RsiChart::leaveEvent(QEvent* event)
{
    QChartView::leaveEvent(event);
    hideCrosshair();
    emit crosshairLeft();
}

// ── public slots ──────────────────────────────────────────────────────────────

void RsiChart::updateCrosshair(qint64 timestampMs)
{
    if (m_timestamps.isEmpty()) return;
    const double sx = chart()->mapToPosition(QPointF(timestampMs, 0)).x();
    const QRectF  pa = chart()->plotArea();
    if (sx >= pa.left() && sx <= pa.right())
        paintCrosshairAt(sx, timestampMs);
}

void RsiChart::hideCrosshair()
{
    m_crosshairLine->setVisible(false);
    m_infoBg->setVisible(false);
    m_infoText->setVisible(false);
}

// ── private helpers ───────────────────────────────────────────────────────────

void RsiChart::paintCrosshairAt(double sceneX, qint64 timestampMs)
{
    const QRectF pa = chart()->plotArea();

    m_crosshairLine->setLine(sceneX, pa.top(), sceneX, pa.bottom());
    m_crosshairLine->setVisible(true);

    const int idx = nearestSample(timestampMs);
    if (idx < 0 || std::isnan(m_values[idx])) return;

    m_infoText->setPlainText(QString("RSI: %1").arg(m_values[idx], 0, 'f', 1));

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

int RsiChart::nearestSample(qint64 tsMs) const
{
    if (m_timestamps.isEmpty()) return -1;
    int lo = 0, hi = m_timestamps.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (m_timestamps[mid].toMSecsSinceEpoch() < tsMs)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo > 0) {
        const qint64 d0 = qAbs(m_timestamps[lo-1].toMSecsSinceEpoch() - tsMs);
        const qint64 d1 = qAbs(m_timestamps[lo].toMSecsSinceEpoch()   - tsMs);
        if (d0 < d1) --lo;
    }
    return lo;
}
