#pragma once

#include "Candle.h"
#include "CollapsiblePanel.h"

class QLabel;

class MarketPanel : public CollapsiblePanel {
    Q_OBJECT
public:
    explicit MarketPanel(QWidget* parent = nullptr);
    void update(const Candle& last);

private:
    QLabel* m_date  = nullptr;
    QLabel* m_open  = nullptr;
    QLabel* m_high  = nullptr;
    QLabel* m_low   = nullptr;
    QLabel* m_close = nullptr;
    QLabel* m_vol   = nullptr;
};
