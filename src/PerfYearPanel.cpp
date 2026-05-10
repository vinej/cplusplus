#include "PerfYearPanel.h"

#include <QDate>
#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
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

static double perfYear(const CandleSeries& candles, int year)
{
    if (candles.isEmpty()) return std::numeric_limits<double>::quiet_NaN();

    int first = -1, last = -1;
    for (int i = 0; i < candles.size(); ++i) {
        if (candles[i].timestamp.date().year() == year) {
            if (first == -1) first = i;
            last = i;
        }
    }
    if (first == -1 || last == -1 || first == last)
        return std::numeric_limits<double>::quiet_NaN();

    const double startAdj = (first > 0) ? candles[first - 1].adjClose : candles[first].adjClose;
    if (startAdj == 0.0) return std::numeric_limits<double>::quiet_NaN();
    return (candles[last].adjClose - startAdj) / startAdj * 100.0;
}

// ── PerfYearPanel ─────────────────────────────────────────────────────────────

PerfYearPanel::PerfYearPanel(QWidget* parent)
    : CollapsiblePanel("Performance by Year", parent)
{
}

void PerfYearPanel::update(const CandleSeries& history)
{
    if (history.isEmpty()) return;

    if (QLayout* old = body()->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete old;
    }

    const QDate today       = QDate::currentDate();
    const int   currentYear = today.year();
    const int   startYear   = today.addYears(-26).year();

    m_grid = new QGridLayout(body());
    m_grid->setContentsMargins(8, 4, 8, 4);
    m_grid->setHorizontalSpacing(8);
    m_grid->setVerticalSpacing(3);

    constexpr int kCols = 5;
    int col = 0, row = 0;

    for (int y = currentYear; y >= startYear; --y) {
        const double pct = perfYear(history, y);

        auto* box = new QWidget(body());
        auto* vl  = new QVBoxLayout(box);
        vl->setContentsMargins(2, 1, 2, 1);
        vl->setSpacing(0);

        QString yearLabel = QString::number(y);
        if (y == currentYear) yearLabel += " YTD";

        auto* yearLbl = new QLabel(yearLabel, box);
        yearLbl->setAlignment(Qt::AlignCenter);
        yearLbl->setStyleSheet("color: gray; font-size: 10px;");

        auto* retLbl = new QLabel("—", box);
        retLbl->setAlignment(Qt::AlignCenter);
        applyReturnStyle(retLbl, pct);

        vl->addWidget(yearLbl);
        vl->addWidget(retLbl);

        m_grid->addWidget(box, row, col);
        ++col;
        if (col >= kCols) { col = 0; ++row; }
    }
}

void PerfYearPanel::update(const QMap<int,double>& returnsByYear)
{
    if (returnsByYear.isEmpty()) return;

    if (QLayout* old = body()->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete old;
    }

    const QDate today       = QDate::currentDate();
    const int   currentYear = today.year();
    const int   startYear   = today.addYears(-26).year();

    m_grid = new QGridLayout(body());
    m_grid->setContentsMargins(8, 4, 8, 4);
    m_grid->setHorizontalSpacing(8);
    m_grid->setVerticalSpacing(3);

    constexpr int kCols = 5;
    int col = 0, row = 0;

    for (int y = currentYear; y >= startYear; --y) {
        auto* box = new QWidget(body());
        auto* vl  = new QVBoxLayout(box);
        vl->setContentsMargins(2, 1, 2, 1);
        vl->setSpacing(0);

        QString yearLabel = QString::number(y);
        if (y == currentYear) yearLabel += " YTD";

        auto* yearLbl = new QLabel(yearLabel, box);
        yearLbl->setAlignment(Qt::AlignCenter);
        yearLbl->setStyleSheet("color: gray; font-size: 10px;");

        auto* retLbl = new QLabel("—", box);
        retLbl->setAlignment(Qt::AlignCenter);
        applyReturnStyle(retLbl, returnsByYear.value(y, std::numeric_limits<double>::quiet_NaN()));

        vl->addWidget(yearLbl);
        vl->addWidget(retLbl);

        m_grid->addWidget(box, row, col);
        ++col;
        if (col >= kCols) { col = 0; ++row; }
    }
}
