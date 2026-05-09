#pragma once

#include "Candle.h"
#include "CollapsiblePanel.h"

class QLabel;

class PerfSincePanel : public CollapsiblePanel {
    Q_OBJECT
public:
    explicit PerfSincePanel(QWidget* parent = nullptr);
    void update(const CandleSeries& history,
                const CandleSeries& maxCandles = {},
                bool maxCandlesValid = false);

private:
    QLabel* m_perf[9] = {};
};
