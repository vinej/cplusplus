#include "PortfolioAnalysisWidget.h"

#include "MarketPanel.h"
#include "PerfSincePanel.h"
#include "PerfYearPanel.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTime>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

// ── symbol-line color palette ─────────────────────────────────────────────────

static const QColor kPalette[] = {
    QColor("#2196F3"), QColor("#FF9800"), QColor("#4CAF50"), QColor("#E91E63"),
    QColor("#9C27B0"), QColor("#00BCD4"), QColor("#FF5722"), QColor("#607D8B"),
};
static constexpr int kPaletteSize = static_cast<int>(sizeof(kPalette)/sizeof(*kPalette));

// ── price helpers ─────────────────────────────────────────────────────────────

// Last available closing price on or before `date` (floor search over sorted QMap).
static double priceAtOrBefore(const QMap<QDate,double>& prices, const QDate& date)
{
    if (prices.isEmpty()) return 0.0;
    auto it = prices.upperBound(date); // first key > date
    if (it == prices.begin()) return 0.0;
    --it;
    return it.value();
}

// Method-2 portfolio return from startDate to endDate.
// Each symbol contributes weight_i * (end_price_i / start_price_i - 1).
// Weights are re-normalised to only symbols that have valid prices at both ends.
static double portfolioReturn(
    const QMap<QString, QMap<QDate,double>>& priceMaps,
    const QMap<QString,double>& weights,
    const QDate& startDate,
    const QDate& endDate)
{
    double usableWeight = 0.0, ret = 0.0;
    for (auto it = weights.cbegin(); it != weights.cend(); ++it) {
        const auto pm = priceMaps.find(it.key());
        if (pm == priceMaps.cend()) continue;
        const double sp = priceAtOrBefore(*pm, startDate);
        const double ep = priceAtOrBefore(*pm, endDate);
        if (sp <= 0.0 || ep <= 0.0) continue;
        ret          += it.value() * (ep / sp - 1.0) * 100.0;
        usableWeight += it.value();
    }
    if (usableWeight <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    return ret / usableWeight; // normalise to available symbols
}

// ── PortfolioAnalysisWidget ───────────────────────────────────────────────────

PortfolioAnalysisWidget::PortfolioAnalysisWidget(QWidget* parent)
    : QWidget(parent)
    , m_portfolioCombo(new QComboBox(this))
    , m_rangeCombo(new QComboBox(this))
    , m_intervalCombo(new QComboBox(this))
    , m_fetchBtn(new QPushButton("Fetch", this))
    , m_chart(new PerformanceChart(this))
    , m_marketPanel(new MarketPanel(this))
    , m_perfSincePanel(new PerfSincePanel(this))
    , m_perfYearPanel(new PerfYearPanel(this))
    , m_client(new YahooFinanceClient(this))
{
    m_portfolioCombo->setMinimumWidth(180);

    m_rangeCombo->addItems({"1d","5d","1mo","3mo","6mo","1y","2y","5y","max"});
    m_rangeCombo->setCurrentText("1y");

    m_intervalCombo->addItems({"1m","5m","15m","30m","60m","90m","1d","1wk","1mo"});
    m_intervalCombo->setCurrentText("1d");

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel("Portfolio:", this));
    topRow->addWidget(m_portfolioCombo);
    topRow->addWidget(new QLabel("Range:", this));
    topRow->addWidget(m_rangeCombo);
    topRow->addWidget(new QLabel("Interval:", this));
    topRow->addWidget(m_intervalCombo);
    topRow->addWidget(m_fetchBtn);
    topRow->addStretch(1);

    auto* panelsRow = new QHBoxLayout();
    panelsRow->setSpacing(4);
    panelsRow->addWidget(m_marketPanel,    1);
    panelsRow->addWidget(m_perfSincePanel, 2);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->addLayout(topRow);
    root->addLayout(panelsRow);
    root->addWidget(m_perfYearPanel);
    root->addWidget(m_chart, 1);

    connect(m_fetchBtn, &QPushButton::clicked, this, &PortfolioAnalysisWidget::onFetch);
    connect(m_client, &YahooFinanceClient::finished,
            this,     &PortfolioAnalysisWidget::onFetchFinished);
    connect(m_client, &YahooFinanceClient::failed,
            this,     &PortfolioAnalysisWidget::onFetchFailed);

    PortfolioDb::instance().open();
    loadPortfolios();
}

// ── portfolio list ────────────────────────────────────────────────────────────

void PortfolioAnalysisWidget::loadPortfolios()
{
    const int savedId = m_portfolioCombo->currentData().toInt();

    m_portfolioCombo->blockSignals(true);
    m_portfolioCombo->clear();
    m_portfolioCombo->addItem("— Select portfolio —", -1);
    for (const auto& p : PortfolioDb::instance().portfolios())
        m_portfolioCombo->addItem(p.second, p.first);

    // Restore previously selected portfolio if still present.
    if (savedId > 0) {
        for (int i = 0; i < m_portfolioCombo->count(); ++i) {
            if (m_portfolioCombo->itemData(i).toInt() == savedId) {
                m_portfolioCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    m_portfolioCombo->blockSignals(false);
}

// ── fetch ─────────────────────────────────────────────────────────────────────

QDate PortfolioAnalysisWidget::startDate() const
{
    const QDate today = QDate::currentDate();
    const QString r   = m_rangeCombo->currentText();
    if (r == "1d")  return today.addDays(-1);
    if (r == "5d")  return today.addDays(-5);
    if (r == "1mo") return today.addMonths(-1);
    if (r == "3mo") return today.addMonths(-3);
    if (r == "6mo") return today.addMonths(-6);
    if (r == "1y")  return today.addYears(-1);
    if (r == "2y")  return today.addYears(-2);
    if (r == "5y")  return today.addYears(-5);
    return QDate(1970, 1, 1); // "max"
}

void PortfolioAnalysisWidget::onFetch()
{
    const int portfolioId = m_portfolioCombo->currentData().toInt();
    if (portfolioId < 0) return;

    m_positions = PortfolioDb::instance().positions(portfolioId);
    m_symbolCandles.clear();
    m_historyPrices.clear();

    if (m_positions.isEmpty()) {
        emit statusMessage("Portfolio has no positions.");
        return;
    }

    QStringList symbols;
    for (const auto& pos : m_positions)
        if (!symbols.contains(pos.symbol)) symbols << pos.symbol;

    m_pendingChart   = symbols.size();
    m_pendingHistory = symbols.size();
    m_fetchBtn->setEnabled(false);

    const QDate today    = QDate::currentDate();
    const QString intvl  = m_intervalCombo->currentText();
    const QDate   start  = startDate();

    emit statusMessage(QString("Fetching %1 symbol(s) for portfolio analysis...")
                           .arg(symbols.size()));

    for (const QString& sym : symbols) {
        // Chart data (selected range/interval) — for the performance chart.
        m_client->fetch(sym, intvl,  start,                today, "portperf:"    + sym);
        // 27-year daily history — for PerfSince and PerfByYear panels.
        m_client->fetch(sym, "1d",   today.addYears(-27),  today, "porthistory:" + sym);
    }
}

void PortfolioAnalysisWidget::onFetchFinished(const QString& symbol,
                                               const QString& tag,
                                               const CandleSeries& candles,
                                               const QString& /*name*/)
{
    if (tag.startsWith("portperf:")) {
        m_symbolCandles[symbol] = candles;
        if (--m_pendingChart <= 0) {
            renderChart();
            if (m_pendingHistory <= 0)
                emit statusMessage("Portfolio analysis complete.");
            else
                emit statusMessage("Chart ready — loading history for panels...");
        }
        return;
    }

    if (tag.startsWith("porthistory:")) {
        auto& pm = m_historyPrices[symbol];
        for (const auto& c : candles)
            pm[c.timestamp.date()] = c.close;

        if (--m_pendingHistory <= 0) {
            m_fetchBtn->setEnabled(true);
            renderPanels();
            emit statusMessage("Portfolio analysis complete.");
        }
    }
}

void PortfolioAnalysisWidget::onFetchFailed(const QString& symbol,
                                             const QString& tag,
                                             const QString& message)
{
    emit statusMessage(QString("%1 [%2] failed: %3").arg(symbol, tag, message));

    if (tag.startsWith("portperf:")) {
        if (--m_pendingChart <= 0) renderChart();
    } else if (tag.startsWith("porthistory:")) {
        if (--m_pendingHistory <= 0) {
            m_fetchBtn->setEnabled(true);
            renderPanels();
        }
    }
}

// ── weights ───────────────────────────────────────────────────────────────────

QMap<QString,double> PortfolioAnalysisWidget::symbolWeights() const
{
    QMap<QString,double> weights;
    double totalOW = 0.0;
    for (const auto& pos : m_positions) {
        weights[pos.symbol] += pos.originalWeight;
        totalOW             += pos.originalWeight;
    }
    if (totalOW < 0.5) {
        const double eq = 100.0 / qMax(1, weights.size());
        for (auto it = weights.begin(); it != weights.end(); ++it)
            it.value() = eq;
    }
    return weights;
}

// ── render performance chart ──────────────────────────────────────────────────

void PortfolioAnalysisWidget::renderChart()
{
    if (m_symbolCandles.isEmpty()) return;

    const QMap<QString,double> weights = symbolWeights();

    // Build date→price maps from the chart-range candles.
    QMap<QString, QMap<QDate,double>> priceMaps;
    for (auto it = m_symbolCandles.begin(); it != m_symbolCandles.end(); ++it) {
        auto& pm = priceMaps[it.key()];
        for (const auto& c : it.value())
            pm[c.timestamp.date()] = c.close;
    }

    // Date intersection across all symbols.
    QVector<QDate> commonDates;
    bool first = true;
    for (auto it = priceMaps.begin(); it != priceMaps.end(); ++it) {
        if (it.value().isEmpty()) continue;
        if (first) {
            for (auto jt = it.value().begin(); jt != it.value().end(); ++jt)
                commonDates.append(jt.key());
            first = false;
        } else {
            QSet<QDate> other(it.value().keyBegin(), it.value().keyEnd());
            QVector<QDate> intersection;
            for (const QDate& d : commonDates)
                if (other.contains(d)) intersection.append(d);
            commonDates = intersection;
        }
    }
    std::sort(commonDates.begin(), commonDates.end());
    if (commonDates.isEmpty()) return;

    // Initial prices at first common date.
    QMap<QString,double> initPrices;
    for (auto it = priceMaps.begin(); it != priceMaps.end(); ++it)
        initPrices[it.key()] = it.value().value(commonDates.first(), 0.0);

    // Usable weight = sum of weights for symbols with a valid initial price.
    double usableWeight = 0.0;
    for (auto it = weights.begin(); it != weights.end(); ++it)
        if (initPrices.value(it.key(), 0.0) > 0.0) usableWeight += it.value();

    QVector<PerformanceChart::Series> chartSeries;
    int colorIdx = 0;

    for (auto it = priceMaps.begin(); it != priceMaps.end(); ++it) {
        const QString& sym  = it.key();
        const double   init = initPrices.value(sym, 0.0);
        if (init <= 0.0) continue;

        PerformanceChart::Series s;
        s.label     = sym;
        s.color     = kPalette[colorIdx++ % kPaletteSize];
        s.lineWidth = 1.5;

        for (const QDate& d : commonDates) {
            const double price = it.value().value(d, 0.0);
            if (price <= 0.0) continue;
            s.timestamps.append(QDateTime(d, QTime(0,0,0), Qt::UTC));
            s.returns.append((price / init - 1.0) * 100.0);
        }
        chartSeries.append(s);
    }

    // Portfolio aggregate (black, bold).
    {
        PerformanceChart::Series port;
        port.label     = "Portfolio";
        port.color     = Qt::black;
        port.lineWidth = 2.5;

        for (const QDate& d : commonDates) {
            double portRet = 0.0, used = 0.0;
            for (auto it = weights.begin(); it != weights.end(); ++it) {
                const double init  = initPrices.value(it.key(), 0.0);
                if (init <= 0.0 || usableWeight <= 0.0) continue;
                const double price = priceMaps[it.key()].value(d, 0.0);
                if (price <= 0.0) continue;
                portRet += (it.value() / usableWeight) * (price / init - 1.0) * 100.0;
                used    += it.value();
            }
            port.timestamps.append(QDateTime(d, QTime(0,0,0), Qt::UTC));
            port.returns.append(used > 0.0 ? portRet
                                           : std::numeric_limits<double>::quiet_NaN());
        }
        chartSeries.append(port);
    }

    m_chart->setSeries(chartSeries);

    // Update MarketPanel with the last portfolio-index candle.
    if (!commonDates.isEmpty()) {
        const QDate last = commonDates.last();
        double portRet = 0.0, used = 0.0;
        for (auto it = weights.begin(); it != weights.end(); ++it) {
            const double init  = initPrices.value(it.key(), 0.0);
            if (init <= 0.0 || usableWeight <= 0.0) continue;
            const double price = priceMaps[it.key()].value(last, 0.0);
            if (price <= 0.0) continue;
            portRet += (it.value() / usableWeight) * (price / init - 1.0) * 100.0;
            used    += it.value();
        }
        Candle c;
        c.timestamp = QDateTime(last, QTime(0,0,0), Qt::UTC);
        c.close = c.open = c.high = c.low = c.adjClose =
            100.0 * (1.0 + (used > 0.0 ? portRet / 100.0 : 0.0));
        c.volume = 0;
        m_marketPanel->update(c);
    }
}

// ── render panels from 27y history ───────────────────────────────────────────

void PortfolioAnalysisWidget::renderPanels()
{
    if (m_historyPrices.isEmpty()) return;

    const QMap<QString,double> weights = symbolWeights();
    const QDate today = QDate::currentDate();

    // ── PerfSincePanel ────────────────────────────────────────────────────────
    auto portRet = [&](const QDate& from) {
        return portfolioReturn(m_historyPrices, weights, from, today);
    };

    // Earliest date where any symbol has history → "max" start.
    QDate maxStart = today;
    for (auto it = m_historyPrices.begin(); it != m_historyPrices.end(); ++it)
        if (!it.value().isEmpty()) maxStart = qMin(maxStart, it.value().firstKey());

    const QVector<double> sinceReturns = {
        portRet(today.addDays(-30)),
        portRet(today.addDays(-91)),
        portRet(today.addDays(-182)),
        portRet(today.addYears(-1)),
        portRet(today.addYears(-2)),
        portRet(today.addYears(-5)),
        portRet(today.addYears(-10)),
        portRet(today.addYears(-20)),
        portRet(maxStart),
    };
    m_perfSincePanel->update(sinceReturns);

    // ── PerfYearPanel ─────────────────────────────────────────────────────────
    // For year Y: start = last price of Y-1 (Dec 31 Y-1), end = last price of Y.
    const int startYear = today.addYears(-26).year();
    QMap<int,double> yearReturns;
    for (int y = today.year(); y >= startYear; --y) {
        const QDate yStart = QDate(y - 1, 12, 31);
        const QDate yEnd   = (y < today.year()) ? QDate(y, 12, 31) : today;
        yearReturns[y]     = portfolioReturn(m_historyPrices, weights, yStart, yEnd);
    }
    m_perfYearPanel->update(yearReturns);
}
