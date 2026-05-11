#include "AnalysisWidget.h"

#include "CandleChart.h"
#include "Indicators.h"
#include "MarketPanel.h"
#include "PerfSincePanel.h"
#include "PerfYearPanel.h"
#include "PortfolioAnalysisWidget.h"
#include "RsiChart.h"
#include "SymbolPicker.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

// ── AnalysisWidget ────────────────────────────────────────────────────────────

AnalysisWidget::AnalysisWidget(QWidget* parent)
    : QWidget(parent)
    , m_client(new YahooFinanceClient(this))
    , m_chart(new CandleChart(this))
    , m_rsiChart(new RsiChart(this))
    , m_scrollArea(new QScrollArea(this))
    , m_symbolPicker(new SymbolPicker(this))
    , m_indicatorCombo(new QComboBox(this))
    , m_periodSpin(new QSpinBox(this))
    , m_marketPanel(new MarketPanel(this))
    , m_perfSincePanel(new PerfSincePanel(this))
    , m_perfYearPanel(new PerfYearPanel(this))
{
    m_indicatorCombo->addItems({"None", "SMA", "EMA", "RSI"});
    m_periodSpin->setRange(2, 200);
    m_periodSpin->setValue(20);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(m_symbolPicker);
    topRow->addWidget(new QLabel("Indicator:", this));
    topRow->addWidget(m_indicatorCombo);
    topRow->addWidget(new QLabel("Period:", this));
    topRow->addWidget(m_periodSpin);
    topRow->addStretch(1);

    auto* topPanelsRow = new QHBoxLayout();
    topPanelsRow->setSpacing(4);
    topPanelsRow->addWidget(m_marketPanel,    1);
    topPanelsRow->addWidget(m_perfSincePanel, 2);

    m_chart->setFixedHeight(450);
    m_rsiChart->hide();

    auto* scrollContent = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->setSpacing(4);
    contentLayout->addLayout(topRow);
    contentLayout->addLayout(topPanelsRow);
    contentLayout->addWidget(m_perfYearPanel);
    contentLayout->addWidget(m_chart);
    contentLayout->addWidget(m_rsiChart);
    contentLayout->addStretch(1);

    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // ── "By Symbol" page — wraps all existing content ────────────────────────
    auto* bySymbolPage = new QWidget(this);
    auto* bsRoot = new QVBoxLayout(bySymbolPage);
    bsRoot->setContentsMargins(0, 0, 0, 0);
    bsRoot->addWidget(m_scrollArea, 1);

    // ── "By Portfolio" page ───────────────────────────────────────────────────
    auto* byPortfolioPage = new PortfolioAnalysisWidget(this);
    connect(byPortfolioPage, &PortfolioAnalysisWidget::statusMessage,
            this,            &AnalysisWidget::statusMessage);

    auto* innerTabs = new QTabWidget(this);
    innerTabs->addTab(bySymbolPage,    "By Symbol");
    innerTabs->addTab(byPortfolioPage, "By Portfolio");

    // Refresh the portfolio list whenever the By Portfolio tab is shown,
    // so portfolios created in the Portfolio tab are immediately visible.
    connect(innerTabs, &QTabWidget::currentChanged, this,
            [innerTabs, byPortfolioPage](int index) {
                if (innerTabs->widget(index) == byPortfolioPage)
                    byPortfolioPage->loadPortfolios();
            });

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(innerTabs, 1);

    connect(m_symbolPicker, &SymbolPicker::fetchRequested,
            this,           &AnalysisWidget::onFetchClicked);
    connect(m_symbolPicker, &SymbolPicker::rangeChanged,
            this, [this](const QString&) { fetchChartOnly(); });
    connect(m_symbolPicker, &SymbolPicker::intervalChanged,
            this, [this](const QString&) { fetchChartOnly(); });
    connect(m_indicatorCombo, &QComboBox::currentTextChanged,
            this, [this](const QString&){ applyIndicator(); });
    connect(m_periodSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int){ applyIndicator(); });

    connect(m_client, &YahooFinanceClient::finished,
            this,     &AnalysisWidget::onFetchFinished);
    connect(m_client, &YahooFinanceClient::failed,
            this,     &AnalysisWidget::onFetchFailed);

    connect(m_chart,    &CandleChart::crosshairMoved, m_rsiChart, &RsiChart::updateCrosshair);
    connect(m_chart,    &CandleChart::crosshairLeft,  m_rsiChart, &RsiChart::hideCrosshair);
    connect(m_rsiChart, &RsiChart::crosshairMoved,    m_chart,    &CandleChart::updateCrosshair);
    connect(m_rsiChart, &RsiChart::crosshairLeft,     m_chart,    &CandleChart::hideCrosshair);
}

// ── fetch flow ────────────────────────────────────────────────────────────────

void AnalysisWidget::onFetchClicked()
{
    const QString symbol = m_symbolPicker->symbol();
    if (symbol.isEmpty()) return;

    m_historyCandles.clear();
    m_maxCandles.clear();
    m_maxCandlesValid = false;
    m_symbolPicker->setFetchEnabled(false);

    emit statusMessage(QString("Fetching %1 %2/%3...")
        .arg(symbol, m_symbolPicker->range(), m_symbolPicker->interval()));

    m_client->fetch(symbol, m_symbolPicker->interval(),
                    m_symbolPicker->startDate(), QDate::currentDate(), "chart");
}

void AnalysisWidget::fetchChartOnly()
{
    if (m_lastCandles.isEmpty()) return;
    m_client->fetch(m_symbolPicker->symbol(), m_symbolPicker->interval(),
                    m_symbolPicker->startDate(), QDate::currentDate(), "chart-only");
}

void AnalysisWidget::onFetchFinished(const QString& symbol,
                                      const QString& tag,
                                      const CandleSeries& candles,
                                      const QString& /*name*/)
{
    static const QStringList kIntraday = {"1m","5m","15m","30m","60m","90m"};

    if (tag == "chart" || tag == "chart-only") {
        m_lastCandles = candles;
        m_chart->setData(symbol, m_lastCandles);
        updatePanels();
        applyIndicator();

        if (tag == "chart") {
            m_symbolPicker->setFetchEnabled(true);
            if (!kIntraday.contains(m_symbolPicker->interval())) {
                const QDate today = QDate::currentDate();
                emit statusMessage(
                    QString("%1: %2 bars — loading history...")
                        .arg(symbol).arg(m_lastCandles.size()));
                m_client->fetch(symbol, "1d", today.addYears(-27), today, "history");
            } else {
                emit statusMessage(
                    QString("%1: %2 bars").arg(symbol).arg(m_lastCandles.size()));
            }
        }
    }
    else if (tag == "history") {
        m_historyCandles = candles;
        updatePanels();

        if (m_historyCandles.isEmpty()) {
            emit statusMessage(
                QString("%1: %2 bars | no history").arg(symbol).arg(m_lastCandles.size()));
            return;
        }
        const QDate historyStart = m_historyCandles.first().timestamp.date();
        emit statusMessage(
            QString("%1: %2 bars | history: %3 bars — loading early history...")
                .arg(symbol).arg(m_lastCandles.size()).arg(m_historyCandles.size()));
        m_client->fetch(symbol, "1d", QDate(1970, 1, 1), historyStart.addDays(-1), "max");
    }
    else if (tag == "max") {
        m_maxCandles      = candles;
        m_maxCandlesValid = !candles.isEmpty();
        updatePanels();
        emit statusMessage(
            QString("%1: %2 bars | history: %3 bars | max from %4")
                .arg(symbol).arg(m_lastCandles.size()).arg(m_historyCandles.size())
                .arg(m_maxCandlesValid
                     ? m_maxCandles.first().timestamp.toString("yyyy-MM-dd")
                     : "N/A"));
    }
}

void AnalysisWidget::onFetchFailed(const QString& symbol,
                                    const QString& tag,
                                    const QString& message)
{
    if (tag == "chart") m_symbolPicker->setFetchEnabled(true);
    emit statusMessage(QString("%1 [%2] failed: %3").arg(symbol, tag, message));
}

// ── indicators ────────────────────────────────────────────────────────────────

void AnalysisWidget::applyIndicator()
{
    m_chart->clearOverlays();
    m_rsiChart->clear();
    m_rsiChart->hide();

    if (m_lastCandles.isEmpty()) return;

    const QString name = m_indicatorCombo->currentText();
    if (name == "None") return;

    QVector<double> closes;
    closes.reserve(m_lastCandles.size());
    for (const Candle& c : m_lastCandles) closes.append(c.close);

    const int period = m_periodSpin->value();

    if (name == "RSI") {
        QVector<QDateTime> timestamps;
        timestamps.reserve(m_lastCandles.size());
        for (const Candle& c : m_lastCandles) timestamps.append(c.timestamp);
        m_rsiChart->setData(timestamps, Indicators::rsi(closes, period));
        m_rsiChart->show();
        return;
    }

    QVector<double> values;
    if      (name == "SMA") values = Indicators::sma(closes, period);
    else if (name == "EMA") values = Indicators::ema(closes, period);
    m_chart->addOverlay(QString("%1(%2)").arg(name).arg(period), values);
}

// ── panels ────────────────────────────────────────────────────────────────────

void AnalysisWidget::updatePanels()
{
    if (!m_lastCandles.isEmpty())
        m_marketPanel->update(m_lastCandles.last());

    if (!m_historyCandles.isEmpty()) {
        m_perfSincePanel->update(m_historyCandles, m_maxCandles, m_maxCandlesValid);
        m_perfYearPanel->update(m_historyCandles);
    }
}
