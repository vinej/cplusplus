#include "PerfSincePanel.h"

#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <limits>

static void applyReturnStyle(QLabel* lbl, double pct)
{
    if (std::isnan(pct)) {
        lbl->setText("—");
        lbl->setStyleSheet("color: gray;");
        return;
    }
    const QString sign = (pct >= 0) ? "+" : "";
    lbl->setText(QString("%1%2%").arg(sign).arg(pct, 0, 'f', 2));
    lbl->setStyleSheet(pct >= 0 ? "color: #4caf50; font-weight: bold;"
                                : "color: #f44336; font-weight: bold;");
}

// Floor search: last bar whose date <= targetDate (preceding trading day on weekends/holidays).
static double perfSince(const CandleSeries& candles, const QDate& targetDate)
{
    if (candles.isEmpty()) return std::numeric_limits<double>::quiet_NaN();

    int lo = 0, hi = candles.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (candles[mid].timestamp.date() <= targetDate) lo = mid;
        else                                              hi = mid - 1;
    }
    if (candles[lo].timestamp.date() > targetDate)
        return std::numeric_limits<double>::quiet_NaN();

    const double startAdj = candles[lo].adjClose;
    if (startAdj == 0.0) return std::numeric_limits<double>::quiet_NaN();
    return (candles.last().adjClose - startAdj) / startAdj * 100.0;
}

// ── PerfSincePanel ────────────────────────────────────────────────────────────

PerfSincePanel::PerfSincePanel(QWidget* parent)
    : CollapsiblePanel("Performance Since", parent)
{
    static const char* kLabels[] = {
        "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "20y", "max"
    };
    constexpr int kCount = 9;

    auto* hbox = new QHBoxLayout(body());
    hbox->setContentsMargins(8, 4, 8, 4);
    hbox->setSpacing(8);

    for (int i = 0; i < kCount; ++i) {
        auto* box = new QWidget(body());
        auto* vl  = new QVBoxLayout(box);
        vl->setContentsMargins(4, 2, 4, 2);
        vl->setSpacing(1);

        auto* lbl = new QLabel(kLabels[i], box);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: gray; font-size: 10px;");

        m_perf[i] = new QLabel("—", box);
        m_perf[i]->setAlignment(Qt::AlignCenter);

        vl->addWidget(lbl);
        vl->addWidget(m_perf[i]);
        hbox->addWidget(box);
    }
    hbox->addStretch(1);
}

void PerfSincePanel::update(const CandleSeries& history,
                             const CandleSeries& maxCandles,
                             bool maxCandlesValid)
{
    if (history.isEmpty()) return;

    const QDate lastDate = history.last().timestamp.date();
    applyReturnStyle(m_perf[0], perfSince(history, lastDate.addDays(-30)));
    applyReturnStyle(m_perf[1], perfSince(history, lastDate.addDays(-91)));
    applyReturnStyle(m_perf[2], perfSince(history, lastDate.addDays(-182)));
    applyReturnStyle(m_perf[3], perfSince(history, lastDate.addYears(-1)));
    applyReturnStyle(m_perf[4], perfSince(history, lastDate.addYears(-2)));
    applyReturnStyle(m_perf[5], perfSince(history, lastDate.addYears(-5)));
    applyReturnStyle(m_perf[6], perfSince(history, lastDate.addYears(-10)));
    applyReturnStyle(m_perf[7], perfSince(history, lastDate.addYears(-20)));

    const double endAdj   = history.last().adjClose;
    const double startAdj = (maxCandlesValid && !maxCandles.isEmpty())
        ? maxCandles.first().adjClose
        : history.first().adjClose;
    applyReturnStyle(m_perf[8], (startAdj == 0.0)
        ? std::numeric_limits<double>::quiet_NaN()
        : (endAdj - startAdj) / startAdj * 100.0);
}
