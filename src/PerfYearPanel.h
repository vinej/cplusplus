#pragma once

#include "Candle.h"
#include "CollapsiblePanel.h"

class QGridLayout;

class PerfYearPanel : public CollapsiblePanel {
    Q_OBJECT
public:
    explicit PerfYearPanel(QWidget* parent = nullptr);
    void update(const CandleSeries& history);
    // Pre-computed overload for portfolio view (year → return %, NaN = no data).
    void update(const QMap<int,double>& returnsByYear);

private:
    QGridLayout* m_grid = nullptr;
};
