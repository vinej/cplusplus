#pragma once

#include <QChartView>
#include <QColor>
#include <QDateTime>
#include <QVector>

class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsTextItem;

class PerformanceChart : public QChartView {
    Q_OBJECT
public:
    struct Series {
        QString            label;
        QColor             color;
        qreal              lineWidth = 1.5;
        QVector<QDateTime> timestamps;
        QVector<double>    returns;   // % return from first point (0 at t0)
    };

    explicit PerformanceChart(QWidget* parent = nullptr);

    void setSeries(const QVector<Series>& series);
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
    int  nearestSample(const Series& s, qint64 tsMs) const;

    QVector<Series> m_series;

    QGraphicsLineItem* m_crosshairLine = nullptr;
    QGraphicsRectItem* m_infoBg        = nullptr;
    QGraphicsTextItem* m_infoText      = nullptr;
};
