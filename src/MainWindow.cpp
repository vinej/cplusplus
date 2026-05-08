#include "MainWindow.h"

#include "CandleChart.h"
#include "Indicators.h"
#include "RsiChart.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_client(new YahooFinanceClient(this))
    , m_chart(new CandleChart(this))
    , m_rsiChart(new RsiChart(this))
    , m_scrollArea(new QScrollArea(this))
    , m_symbolEdit(new QLineEdit("AAPL", this))
    , m_rangeCombo(new QComboBox(this))
    , m_indicatorCombo(new QComboBox(this))
    , m_periodSpin(new QSpinBox(this))
    , m_fetchButton(new QPushButton("Fetch", this))
{
    setWindowTitle("Qt Finance");
    resize(1100, 700);

    m_rangeCombo->addItems({"1mo", "3mo", "6mo", "1y", "2y", "5y", "max"});
    m_rangeCombo->setCurrentText("1y");

    m_indicatorCombo->addItems({"None", "SMA", "EMA", "RSI"});
    m_periodSpin->setRange(2, 200);
    m_periodSpin->setValue(20);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel("Symbol:", this));
    topRow->addWidget(m_symbolEdit);
    topRow->addWidget(new QLabel("Range:", this));
    topRow->addWidget(m_rangeCombo);
    topRow->addWidget(new QLabel("Indicator:", this));
    topRow->addWidget(m_indicatorCombo);
    topRow->addWidget(new QLabel("Period:", this));
    topRow->addWidget(m_periodSpin);
    topRow->addWidget(m_fetchButton);
    topRow->addStretch(1);

    m_rsiChart->hide();

    auto* chartsContainer = new QWidget();
    auto* chartsLayout = new QVBoxLayout(chartsContainer);
    chartsLayout->setContentsMargins(0, 0, 0, 0);
    chartsLayout->setSpacing(0);
    chartsLayout->addWidget(m_chart);
    chartsLayout->addWidget(m_rsiChart);

    m_scrollArea->setWidget(chartsContainer);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->installEventFilter(this);

    auto* root = new QVBoxLayout();
    root->addLayout(topRow);
    root->addWidget(m_scrollArea, /*stretch*/ 1);

    auto* central = new QWidget(this);
    central->setLayout(root);
    setCentralWidget(central);

    statusBar()->showMessage("Ready");

    connect(m_fetchButton, &QPushButton::clicked,
            this,          &MainWindow::onFetchClicked);
    connect(m_symbolEdit,  &QLineEdit::returnPressed,
            this,          &MainWindow::onFetchClicked);
    connect(m_indicatorCombo, &QComboBox::currentTextChanged,
            this, [this](const QString&){ applyIndicator(); });
    connect(m_periodSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int){ applyIndicator(); });

    connect(m_client, &YahooFinanceClient::finished,
            this,     &MainWindow::onDataReady);
    connect(m_client, &YahooFinanceClient::failed,
            this,     &MainWindow::onDataFailed);

    // Sync crosshair between the two chart panels
    connect(m_chart,    &CandleChart::crosshairMoved, m_rsiChart, &RsiChart::updateCrosshair);
    connect(m_chart,    &CandleChart::crosshairLeft,  m_rsiChart, &RsiChart::hideCrosshair);
    connect(m_rsiChart, &RsiChart::crosshairMoved,    m_chart,    &CandleChart::updateCrosshair);
    connect(m_rsiChart, &RsiChart::crosshairLeft,     m_chart,    &CandleChart::hideCrosshair);
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
    statusBar()->showMessage(QString("Fetching %1 ...").arg(symbol));
    m_fetchButton->setEnabled(false);
    m_client->fetchDaily(symbol, m_rangeCombo->currentText());
}

void MainWindow::onDataReady(const QString& symbol, const CandleSeries& candles)
{
    m_fetchButton->setEnabled(true);
    m_lastCandles = candles;
    m_chart->setData(symbol, candles);
    applyIndicator();
    statusBar()->showMessage(
        QString("%1: %2 bars").arg(symbol).arg(candles.size()));
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
