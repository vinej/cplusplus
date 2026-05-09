#include "AnalysisWidget.h"

#include "CandleChart.h"
#include "Indicators.h"
#include "MarketPanel.h"
#include "PerfSincePanel.h"
#include "PerfYearPanel.h"
#include "RsiChart.h"
#include "SymbolPicker.h"
#include "VolumeChart.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

// ── AnalysisWidget ────────────────────────────────────────────────────────────

AnalysisWidget::AnalysisWidget(QWidget* parent)
    : QWidget(parent)
    , m_client(new YahooFinanceClient(this))
    , m_chart(new CandleChart(this))
    , m_volumeChart(new VolumeChart(this))
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

    m_volumeChart->hide();
    m_rsiChart->hide();

    auto* chartsContainer = new QWidget(this);
    auto* chartsLayout = new QVBoxLayout(chartsContainer);
    chartsLayout->setContentsMargins(0, 0, 0, 0);
    chartsLayout->setSpacing(0);
    chartsLayout->addWidget(m_chart);
    chartsLayout->addWidget(m_volumeChart);
    chartsLayout->addWidget(m_rsiChart);

    m_scrollArea->setWidget(chartsContainer);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->installEventFilter(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->addLayout(topRow);
    root->addLayout(topPanelsRow);
    root->addWidget(m_perfYearPanel);
    root->addWidget(m_scrollArea, 1);

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

    connect(m_chart,       &CandleChart::crosshairMoved,  m_volumeChart, &VolumeChart::updateCrosshair);
    connect(m_chart,       &CandleChart::crosshairLeft,   m_volumeChart, &VolumeChart::hideCrosshair);
    connect(m_chart,       &CandleChart::crosshairMoved,  m_rsiChart,    &RsiChart::updateCrosshair);
    connect(m_chart,       &CandleChart::crosshairLeft,   m_rsiChart,    &RsiChart::hideCrosshair);

    connect(m_volumeChart, &VolumeChart::crosshairMoved,  m_chart,       &CandleChart::updateCrosshair);
    connect(m_volumeChart, &VolumeChart::crosshairLeft,   m_chart,       &CandleChart::hideCrosshair);
    connect(m_volumeChart, &VolumeChart::crosshairMoved,  m_rsiChart,    &RsiChart::updateCrosshair);
    connect(m_volumeChart, &VolumeChart::crosshairLeft,   m_rsiChart,    &RsiChart::hideCrosshair);

    connect(m_rsiChart,    &RsiChart::crosshairMoved,     m_chart,       &CandleChart::updateCrosshair);
    connect(m_rsiChart,    &RsiChart::crosshairLeft,      m_chart,       &CandleChart::hideCrosshair);
    connect(m_rsiChart,    &RsiChart::crosshairMoved,     m_volumeChart, &VolumeChart::updateCrosshair);
    connect(m_rsiChart,    &RsiChart::crosshairLeft,      m_volumeChart, &VolumeChart::hideCrosshair);
}

bool AnalysisWidget::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == m_scrollArea->viewport() && e->type() == QEvent::Resize) {
        const int h = static_cast<QResizeEvent*>(e)->size().height();
        if (h > 0) m_chart->setFixedHeight(h);
    }
    return QWidget::eventFilter(obj, e);
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
                                      const CandleSeries& candles)
{
    static const QStringList kIntraday = {"1m","5m","15m","30m","60m","90m"};

    if (tag == "chart" || tag == "chart-only") {
        m_lastCandles = candles;
        m_chart->setData(symbol, m_lastCandles);
        m_volumeChart->setData(m_lastCandles);
        m_volumeChart->show();
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
