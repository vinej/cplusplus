#pragma once

#include "Candle.h"
#include "CollapsiblePanel.h"

class QGridLayout;

class PerfYearPanel : public CollapsiblePanel {
    Q_OBJECT
public:
    explicit PerfYearPanel(QWidget* parent = nullptr);
    void update(const CandleSeries& history);

private:
    QGridLayout* m_grid = nullptr;
};
