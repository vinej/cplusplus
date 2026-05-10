#include "PerformanceChart.h"

#include <QChart>
#include <QDateTimeAxis>
#include <QFont>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QLineSeries>
#include <QMouseEvent>
#include <QPen>
#include <QValueAxis>

#include <cmath>
#include <limits>

PerformanceChart::PerformanceChart(QWidget* parent)
    : QChartView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    chart()->setAnimationOptions(QChart::NoAnimation);
    chart()->setTitle("Portfolio Performance (% return from start)");
    chart()->legend()->setVisible(true);
    chart()->legend()->setAlignment(Qt::AlignBottom);
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

void PerformanceChart::clear()
{
    m_series.clear();
    hideCrosshair();
    QChart* c = chart();
    c->removeAllSeries();
    for (QAbstractAxis* a : c->axes()) c->removeAxis(a);
}

void PerformanceChart::setSeries(const QVector<Series>& series)
{
    clear();
    m_series = series;
    if (series.isEmpty()) return;

    qint64 tMin = std::numeric_limits<qint64>::max();
    qint64 tMax = std::numeric_limits<qint64>::min();
    double yMin = 0.0, yMax = 0.0;

    for (const auto& s : series) {
        if (s.timestamps.isEmpty()) continue;
        tMin = qMin(tMin, s.timestamps.first().toMSecsSinceEpoch());
        tMax = qMax(tMax, s.timestamps.last().toMSecsSinceEpoch());
        for (double v : s.returns) {
            if (!std::isnan(v)) { yMin = qMin(yMin, v); yMax = qMax(yMax, v); }
        }
    }

    const double yPad = qMax(2.0, (yMax - yMin) * 0.05);
    yMin -= yPad;
    yMax += yPad;

    QChart* c = chart();
    for (const auto& s : series) {
        if (s.timestamps.isEmpty()) continue;
        auto* line = new QLineSeries();
        line->setName(s.label);
        QPen pen(s.color);
        pen.setWidthF(s.lineWidth);
        line->setPen(pen);
        for (int i = 0; i < s.timestamps.size(); ++i) {
            if (!std::isnan(s.returns[i]))
                line->append(s.timestamps[i].toMSecsSinceEpoch(), s.returns[i]);
        }
        c->addSeries(line);
    }

    auto* xAxis = new QDateTimeAxis();
    xAxis->setFormat("MMM yy");
    xAxis->setRange(QDateTime::fromMSecsSinceEpoch(tMin),
                    QDateTime::fromMSecsSinceEpoch(tMax));
    c->addAxis(xAxis, Qt::AlignBottom);

    auto* yAxis = new QValueAxis();
    yAxis->setRange(yMin, yMax);
    yAxis->setLabelFormat("%.1f%%");
    yAxis->setTitleText("Return %");
    c->addAxis(yAxis, Qt::AlignLeft);

    for (QAbstractSeries* s : c->series()) {
        s->attachAxis(xAxis);
        s->attachAxis(yAxis);
    }
}

// ── mouse events ──────────────────────────────────────────────────────────────

void PerformanceChart::mouseMoveEvent(QMouseEvent* event)
{
    QChartView::mouseMoveEvent(event);
    if (m_series.isEmpty()) return;

    const QPointF sp = mapToScene(event->pos());
    const QRectF  pa = chart()->plotArea();
    if (!pa.contains(sp)) { hideCrosshair(); return; }

    const qint64 tsMs = static_cast<qint64>(chart()->mapToValue(sp).x());
    paintCrosshairAt(sp.x(), tsMs);
    emit crosshairMoved(tsMs);
}

void PerformanceChart::leaveEvent(QEvent* event)
{
    QChartView::leaveEvent(event);
    hideCrosshair();
    emit crosshairLeft();
}

// ── public slots ──────────────────────────────────────────────────────────────

void PerformanceChart::updateCrosshair(qint64 tsMs)
{
    if (m_series.isEmpty()) return;
    const double sx = chart()->mapToPosition(QPointF(tsMs, 0)).x();
    const QRectF  pa = chart()->plotArea();
    if (sx >= pa.left() && sx <= pa.right())
        paintCrosshairAt(sx, tsMs);
}

void PerformanceChart::hideCrosshair()
{
    m_crosshairLine->setVisible(false);
    m_infoBg->setVisible(false);
    m_infoText->setVisible(false);
}

// ── private helpers ───────────────────────────────────────────────────────────

void PerformanceChart::paintCrosshairAt(double sceneX, qint64 tsMs)
{
    const QRectF pa = chart()->plotArea();
    m_crosshairLine->setLine(sceneX, pa.top(), sceneX, pa.bottom());
    m_crosshairLine->setVisible(true);

    QString text;
    bool headerSet = false;
    for (const auto& s : m_series) {
        if (s.timestamps.isEmpty()) continue;
        const int idx = nearestSample(s, tsMs);
        if (idx < 0) continue;
        if (!headerSet) {
            text = s.timestamps[idx].toString("yyyy-MM-dd") + "\n";
            headerSet = true;
        }
        const double r = s.returns[idx];
        text += QString("%1  %2%3%\n")
                    .arg(s.label, -10)
                    .arg(r >= 0 ? "+" : "")
                    .arg(r, 0, 'f', 2);
    }
    if (text.isEmpty()) return;
    text.chop(1);

    m_infoText->setPlainText(text);
    const QRectF tb = m_infoText->boundingRect();

    // Tooltip stays fixed at top-left of the plot area.
    constexpr qreal margin = 8;
    const qreal bx = pa.left() + margin;
    const qreal by = pa.top()  + margin;
    m_infoBg->setRect(bx - 3, by - 3, tb.width() + 6, tb.height() + 6);
    m_infoText->setPos(bx, by);
    m_infoBg->setVisible(true);
    m_infoText->setVisible(true);
}

int PerformanceChart::nearestSample(const Series& s, qint64 tsMs) const
{
    if (s.timestamps.isEmpty()) return -1;
    int lo = 0, hi = s.timestamps.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (s.timestamps[mid].toMSecsSinceEpoch() < tsMs) lo = mid + 1;
        else                                               hi = mid;
    }
    if (lo > 0) {
        const qint64 d0 = qAbs(s.timestamps[lo-1].toMSecsSinceEpoch() - tsMs);
        const qint64 d1 = qAbs(s.timestamps[lo].toMSecsSinceEpoch()   - tsMs);
        if (d0 < d1) --lo;
    }
    return lo;
}
