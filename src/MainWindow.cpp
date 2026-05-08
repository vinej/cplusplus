#include "MainWindow.h"

#include "CandleChart.h"
#include "Indicators.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_client(new YahooFinanceClient(this))
    , m_chart(new CandleChart(this))
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

    auto* root = new QVBoxLayout();
    root->addLayout(topRow);
    root->addWidget(m_chart, /*stretch*/ 1);

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
    if (m_lastCandles.isEmpty()) return;

    const QString name = m_indicatorCombo->currentText();
    if (name == "None") return;

    QVector<double> closes;
    closes.reserve(m_lastCandles.size());
    for (const Candle& c : m_lastCandles) closes.append(c.close);

    const int period = m_periodSpin->value();
    QVector<double> values;
    if      (name == "SMA") values = Indicators::sma(closes, period);
    else if (name == "EMA") values = Indicators::ema(closes, period);
    else if (name == "RSI") values = Indicators::rsi(closes, period);

    m_chart->addOverlay(QString("%1(%2)").arg(name).arg(period), values);
}
