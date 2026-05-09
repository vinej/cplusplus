#include "MainWindow.h"

#include "CandleChart.h"
#include "CollapsiblePanel.h"
#include "Indicators.h"
#include "RsiChart.h"
#include "VolumeChart.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

// ── static helpers ────────────────────────────────────────────────────────────

// Floor search: last bar whose date <= targetDate (handles weekends/holidays by
// using the preceding trading day, matching standard finance convention).
static double perfSince(const CandleSeries& candles, const QDate& targetDate)
{
    if (candles.isEmpty()) return std::numeric_limits<double>::quiet_NaN();

    int lo = 0, hi = candles.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;  // upper-mid avoids infinite loop for floor search
        if (candles[mid].timestamp.date() <= targetDate)
            lo = mid;
        else
            hi = mid - 1;
    }
    if (candles[lo].timestamp.date() > targetDate)
        return std::numeric_limits<double>::quiet_NaN();

    const double startAdj = candles[lo].adjClose;
    if (startAdj == 0.0) return std::numeric_limits<double>::quiet_NaN();
    return (candles.last().adjClose - startAdj) / startAdj * 100.0;
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

static CandleSeries filterByRange(const CandleSeries& all, const QString& range)
{
    if (range == "max" || all.isEmpty()) return all;

    const qint64 lastTs = all.last().timestamp.toSecsSinceEpoch();
    qint64 cutoff = 0;
    if      (range == "1mo") cutoff = lastTs -   30LL * 86400;
    else if (range == "3mo") cutoff = lastTs -   91LL * 86400;
    else if (range == "6mo") cutoff = lastTs -  182LL * 86400;
    else if (range == "1y")  cutoff = lastTs -  365LL * 86400;
    else if (range == "2y")  cutoff = lastTs -  730LL * 86400;
    else if (range == "5y")  cutoff = lastTs - 1826LL * 86400;
    else                     return all;

    int lo = 0, hi = all.size();
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (all[mid].timestamp.toSecsSinceEpoch() < cutoff)
            lo = mid + 1;
        else
            hi = mid;
    }
    return all.mid(lo);
}

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

// ── MainWindow ────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_client(new YahooFinanceClient(this))
    , m_chart(new CandleChart(this))
    , m_volumeChart(new VolumeChart(this))
    , m_rsiChart(new RsiChart(this))
    , m_scrollArea(new QScrollArea(this))
    , m_symbolEdit(new QLineEdit("AAPL", this))
    , m_rangeCombo(new QComboBox(this))
    , m_intervalCombo(new QComboBox(this))
    , m_indicatorCombo(new QComboBox(this))
    , m_periodSpin(new QSpinBox(this))
    , m_fetchButton(new QPushButton("Fetch", this))
{
    setWindowTitle("Qt Finance");
    resize(1100, 700);

    m_rangeCombo->addItems({"1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "max"});
    m_rangeCombo->setCurrentText("1y");

    m_intervalCombo->addItems({"1m", "5m", "15m", "30m", "60m", "90m", "1d", "1wk", "1mo"});
    m_intervalCombo->setCurrentText("1d");

    m_indicatorCombo->addItems({"None", "SMA", "EMA", "RSI"});
    m_periodSpin->setRange(2, 200);
    m_periodSpin->setValue(20);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel("Symbol:", this));
    topRow->addWidget(m_symbolEdit);
    topRow->addWidget(new QLabel("Range:", this));
    topRow->addWidget(m_rangeCombo);
    topRow->addWidget(new QLabel("Interval:", this));
    topRow->addWidget(m_intervalCombo);
    topRow->addWidget(new QLabel("Indicator:", this));
    topRow->addWidget(m_indicatorCombo);
    topRow->addWidget(new QLabel("Period:", this));
    topRow->addWidget(m_periodSpin);
    topRow->addWidget(m_fetchButton);
    topRow->addStretch(1);

    buildPanels();

    auto* topPanelsRow = new QHBoxLayout();
    topPanelsRow->setSpacing(4);
    topPanelsRow->addWidget(m_marketPanel,    1);
    topPanelsRow->addWidget(m_perfSincePanel, 2);

    m_volumeChart->hide();
    m_rsiChart->hide();

    auto* chartsContainer = new QWidget();
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

    auto* root = new QVBoxLayout();
    root->addLayout(topRow);
    root->addLayout(topPanelsRow);
    root->addWidget(m_perfYearPanel);
    root->addWidget(m_scrollArea, /*stretch*/ 1);

    auto* central = new QWidget(this);
    central->setLayout(root);
    setCentralWidget(central);

    statusBar()->showMessage("Ready");

    connect(m_fetchButton, &QPushButton::clicked,
            this,          &MainWindow::onFetchClicked);
    connect(m_symbolEdit,  &QLineEdit::returnPressed,
            this,          &MainWindow::onFetchClicked);
    connect(m_rangeCombo, &QComboBox::currentTextChanged,
            this, [this](const QString&) {
        if (m_allCandles.isEmpty()) return;
        const QString symbol = m_symbolEdit->text().trimmed().toUpper();
        m_lastCandles = filterByRange(m_allCandles, m_rangeCombo->currentText());
        m_chart->setData(symbol, m_lastCandles);
        m_volumeChart->setData(m_lastCandles);
        applyIndicator();
    });
    connect(m_indicatorCombo, &QComboBox::currentTextChanged,
            this, [this](const QString&){ applyIndicator(); });
    connect(m_periodSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int){ applyIndicator(); });

    connect(m_client, &YahooFinanceClient::finished,
            this,     &MainWindow::onDataReady);
    connect(m_client, &YahooFinanceClient::historyReady,
            this,     &MainWindow::onHistoryReady);
    connect(m_client, &YahooFinanceClient::maxReady,
            this,     &MainWindow::onMaxReady);
    connect(m_client, &YahooFinanceClient::failed,
            this,     &MainWindow::onDataFailed);

    // Sync crosshair across all three panels (no signal loops: updateCrosshair never re-emits)
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

bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == m_scrollArea->viewport() && e->type() == QEvent::Resize) {
        // The viewport size is settled at this point — safe to pin chart to its height
        const int h = static_cast<QResizeEvent*>(e)->size().height();
        if (h > 0) m_chart->setFixedHeight(h);
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::onFetchClicked()
{
    const QString symbol = m_symbolEdit->text().trimmed().toUpper();
    if (symbol.isEmpty()) return;
    const QString range    = m_rangeCombo->currentText();
    const QString interval = m_intervalCombo->currentText();
    statusBar()->showMessage(QString("Fetching %1 %2/%3 ...").arg(symbol, range, interval));
    m_fetchButton->setEnabled(false);
    m_historyCandles.clear();
    m_maxCandles.clear();
    m_maxCandlesValid = false;
    m_client->fetchDaily(symbol, range, interval);
}

void MainWindow::onDataReady(const QString& symbol, const CandleSeries& candles)
{
    m_fetchButton->setEnabled(true);
    m_allCandles = candles;

    m_lastCandles = filterByRange(candles, m_rangeCombo->currentText());

    m_chart->setData(symbol, m_lastCandles);
    m_volumeChart->setData(m_lastCandles);
    m_volumeChart->show();
    updatePanels();
    applyIndicator();

    // For non-intraday intervals launch the 3-chunk daily history fetch chain.
    static const QStringList kIntraday = {"1m", "5m", "15m", "30m", "60m", "90m"};
    if (!kIntraday.contains(m_intervalCombo->currentText())) {
        startHistoryFetch(symbol);
    } else {
        statusBar()->showMessage(
            QString("%1: %2 bars").arg(symbol).arg(m_lastCandles.size()));
    }
}

void MainWindow::startHistoryFetch(const QString& symbol)
{
    // Calendar-based boundaries so leap years are counted exactly.
    const QDate today = QDate::currentDate();
    auto toEpoch = [](QDate d) -> qint64 {
        return QDateTime(d, QTime(0, 0, 0), Qt::UTC).toSecsSinceEpoch();
    };
    const qint64 tNow = QDateTime::currentSecsSinceEpoch();
    const qint64 t9y  = toEpoch(today.addYears(-9));
    const qint64 t17y = toEpoch(today.addYears(-17));
    const qint64 t27y = toEpoch(today.addYears(-27));

    m_historySegments = {
        {t9y,  tNow},  // most recent 9 years
        {t17y, t9y},   // middle 8 years
        {t27y, t17y}   // oldest 10 years (covers Dec base for startYear)
    };
    m_historyCandles.clear();
    statusBar()->showMessage(
        QString("%1: %2 bars — loading history (3 chunks)...").arg(symbol).arg(m_lastCandles.size()));
    const auto seg = m_historySegments.takeFirst();
    m_client->fetchByPeriod(symbol, seg.first, seg.second);
}

void MainWindow::onHistoryReady(const QString& symbol, const CandleSeries& candles)
{
    m_historyCandles.append(candles);

    // Sort chronologically and deduplicate by date after each chunk so that
    // updatePanels() already sees consistent data while loading is in progress.
    std::sort(m_historyCandles.begin(), m_historyCandles.end(),
              [](const Candle& a, const Candle& b){
                  return a.timestamp.toSecsSinceEpoch() < b.timestamp.toSecsSinceEpoch();
              });
    int w = 0;
    for (int i = 0; i < m_historyCandles.size(); ++i) {
        if (w == 0 || m_historyCandles[i].timestamp.date() != m_historyCandles[w-1].timestamp.date())
            m_historyCandles[w++] = m_historyCandles[i];
    }
    m_historyCandles.resize(w);

    if (!m_historySegments.isEmpty()) {
        // More chunks — update panels with what we have, then fire next request.
        updatePanels();
        statusBar()->showMessage(
            QString("%1: %2 bars — history %3 bars, %4 chunk(s) remaining...")
                .arg(symbol).arg(m_lastCandles.size())
                .arg(m_historyCandles.size()).arg(m_historySegments.size()));
        const auto seg = m_historySegments.takeFirst();
        m_client->fetchByPeriod(symbol, seg.first, seg.second);
    } else {
        finalizeHistory(symbol);
    }
}

void MainWindow::finalizeHistory(const QString& symbol)
{
    updatePanels();
    // Fetch daily bars from epoch-0 up to the start of the 27-year history chain.
    // The first bar returned is the IPO-day close in the same daily adjClose series
    // as m_historyCandles, so both endpoints are consistent.
    const qint64 historyStart = m_historyCandles.isEmpty()
        ? 0 : m_historyCandles.first().timestamp.toSecsSinceEpoch();
    statusBar()->showMessage(
        QString("%1: %2 bars | history: %3 daily bars — fetching early history...")
            .arg(symbol).arg(m_lastCandles.size()).arg(m_historyCandles.size()));
    m_client->fetchByPeriod(symbol, 0, historyStart, "1d", "max");
}

void MainWindow::onMaxReady(const QString& symbol, const CandleSeries& candles)
{
    if (!candles.isEmpty()) {
        m_maxCandles      = candles;
        m_maxCandlesValid = true;
    }
    updatePanels();
    statusBar()->showMessage(
        QString("%1: %2 bars | history: %3 daily bars | max from %4")
            .arg(symbol).arg(m_lastCandles.size()).arg(m_historyCandles.size())
            .arg(m_maxCandlesValid
                 ? m_maxCandles.first().timestamp.toString("yyyy-MM-dd")
                 : "N/A"));
}

void MainWindow::onDataFailed(const QString& symbol, const QString& message)
{
    m_fetchButton->setEnabled(true);
    statusBar()->showMessage(QString("%1 failed: %2").arg(symbol, message));
}

void MainWindow::applyIndicator()
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

// ── panel construction ────────────────────────────────────────────────────────

void MainWindow::buildPanels()
{
    // ── Current Market ────────────────────────────────────────────────────────
    m_marketPanel = new CollapsiblePanel("Current Market", this);
    {
        auto* grid = new QGridLayout(m_marketPanel->body());
        grid->setContentsMargins(8, 4, 8, 4);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(2);

        auto makeValueLabel = [](QWidget* parent) {
            auto* l = new QLabel("—", parent);
            l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return l;
        };

        grid->addWidget(new QLabel("Date:",   m_marketPanel->body()), 0, 0);
        m_mktDate = makeValueLabel(m_marketPanel->body());
        grid->addWidget(m_mktDate, 0, 1);

        grid->addWidget(new QLabel("Open:",   m_marketPanel->body()), 1, 0);
        m_mktOpen = makeValueLabel(m_marketPanel->body());
        grid->addWidget(m_mktOpen, 1, 1);

        grid->addWidget(new QLabel("High:",   m_marketPanel->body()), 2, 0);
        m_mktHigh = makeValueLabel(m_marketPanel->body());
        grid->addWidget(m_mktHigh, 2, 1);

        grid->addWidget(new QLabel("Low:",    m_marketPanel->body()), 3, 0);
        m_mktLow = makeValueLabel(m_marketPanel->body());
        grid->addWidget(m_mktLow, 3, 1);

        grid->addWidget(new QLabel("Close:",  m_marketPanel->body()), 4, 0);
        m_mktClose = makeValueLabel(m_marketPanel->body());
        grid->addWidget(m_mktClose, 4, 1);

        grid->addWidget(new QLabel("Volume:", m_marketPanel->body()), 5, 0);
        m_mktVol = makeValueLabel(m_marketPanel->body());
        grid->addWidget(m_mktVol, 5, 1);

        grid->setColumnStretch(1, 1);
    }

    // ── Performance Since ─────────────────────────────────────────────────────
    m_perfSincePanel = new CollapsiblePanel("Performance Since", this);
    {
        static const char* kLabels[] = {
            "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "20y", "max"
        };
        constexpr int kCount = 9;

        auto* hbox = new QHBoxLayout(m_perfSincePanel->body());
        hbox->setContentsMargins(8, 4, 8, 4);
        hbox->setSpacing(8);

        for (int i = 0; i < kCount; ++i) {
            auto* box = new QWidget(m_perfSincePanel->body());
            auto* vl  = new QVBoxLayout(box);
            vl->setContentsMargins(4, 2, 4, 2);
            vl->setSpacing(1);

            auto* lbl = new QLabel(kLabels[i], box);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("color: gray; font-size: 10px;");

            m_perfSince[i] = new QLabel("—", box);
            m_perfSince[i]->setAlignment(Qt::AlignCenter);

            vl->addWidget(lbl);
            vl->addWidget(m_perfSince[i]);
            hbox->addWidget(box);
        }
        hbox->addStretch(1);
    }

    // ── Performance by Year ───────────────────────────────────────────────────
    m_perfYearPanel = new CollapsiblePanel("Performance by Year", this);
    // m_yearGrid is created / rebuilt in updatePanels()
}

// ── panel update ──────────────────────────────────────────────────────────────

void MainWindow::updatePanels()
{
    if (m_allCandles.isEmpty()) return;

    // Current Market — use last available bar (same close regardless of range)
    const Candle& last = m_allCandles.last();
    m_mktDate ->setText(last.timestamp.toString("yyyy-MM-dd"));
    m_mktOpen ->setText(QString::number(last.open,  'f', 2));
    m_mktHigh ->setText(QString::number(last.high,  'f', 2));
    m_mktLow  ->setText(QString::number(last.low,   'f', 2));
    m_mktClose->setText(QString::number(last.close, 'f', 2));
    m_mktVol  ->setText(QLocale().toString(static_cast<qint64>(last.volume)));

    // Performance panels require completed history data.
    if (m_historyCandles.isEmpty()) return;

    // Performance Since — use addYears() for year-based periods so leap years
    // are handled correctly (addDays(-3650) ≠ 10 calendar years).
    const QDate lastDate = m_historyCandles.last().timestamp.date();
    applyReturnStyle(m_perfSince[0], perfSince(m_historyCandles, lastDate.addDays(-30)));
    applyReturnStyle(m_perfSince[1], perfSince(m_historyCandles, lastDate.addDays(-91)));
    applyReturnStyle(m_perfSince[2], perfSince(m_historyCandles, lastDate.addDays(-182)));
    applyReturnStyle(m_perfSince[3], perfSince(m_historyCandles, lastDate.addYears(-1)));
    applyReturnStyle(m_perfSince[4], perfSince(m_historyCandles, lastDate.addYears(-2)));
    applyReturnStyle(m_perfSince[5], perfSince(m_historyCandles, lastDate.addYears(-5)));
    applyReturnStyle(m_perfSince[6], perfSince(m_historyCandles, lastDate.addYears(-10)));
    applyReturnStyle(m_perfSince[7], perfSince(m_historyCandles, lastDate.addYears(-20)));

    // max: earliest daily bar (IPO day) → latest daily bar.
    // m_maxCandles holds pre-history daily bars; falls back to history start for
    // stocks that IPO'd within the 27-year history window.
    {
        const double endAdj   = m_historyCandles.last().adjClose;
        const double startAdj = (m_maxCandlesValid && !m_maxCandles.isEmpty())
            ? m_maxCandles.first().adjClose
            : m_historyCandles.first().adjClose;
        applyReturnStyle(m_perfSince[8], (startAdj == 0.0)
            ? std::numeric_limits<double>::quiet_NaN()
            : (endAdj - startAdj) / startAdj * 100.0);
    }

    // Performance by Year — rebuild grid using full history
    QWidget* body = m_perfYearPanel->body();

    if (QLayout* old = body->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete old;
    }

    const QDate today     = QDate::currentDate();
    const int currentYear = today.year();
    const int startYear   = today.addYears(-26).year();

    m_yearGrid = new QGridLayout(body);
    m_yearGrid->setContentsMargins(8, 4, 8, 4);
    m_yearGrid->setHorizontalSpacing(8);
    m_yearGrid->setVerticalSpacing(3);

    constexpr int kCols = 5;
    int col = 0, row = 0;

    for (int y = currentYear; y >= startYear; --y) {
        const double pct = perfYear(m_historyCandles, y);

        auto* box = new QWidget(body);
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

        m_yearGrid->addWidget(box, row, col);

        ++col;
        if (col >= kCols) { col = 0; ++row; }
    }
}
