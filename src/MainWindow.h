#pragma once

#include "Candle.h"

#include <QMainWindow>
#include <QPair>
#include <QVector>

class CandleChart;
class CollapsiblePanel;
class RsiChart;
class VolumeChart;
class QComboBox;
class QGridLayout;
class QLabel;
class QScrollArea;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStatusBar;
class YahooFinanceClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
    void onFetchClicked();
    void onDataReady(const QString& symbol, const CandleSeries& candles);
    void onHistoryReady(const QString& symbol, const CandleSeries& candles);
    void onMaxReady(const QString& symbol, const CandleSeries& candles);
    void onDataFailed(const QString& symbol, const QString& message);

private:
    void applyIndicator();
    void buildPanels();
    void updatePanels();
    void startHistoryFetch(const QString& symbol);
    void finalizeHistory(const QString& symbol);

    YahooFinanceClient* m_client;
    CandleChart*        m_chart;
    VolumeChart*        m_volumeChart;
    RsiChart*           m_rsiChart;
    QScrollArea*        m_scrollArea;

    QLineEdit*   m_symbolEdit;
    QComboBox*   m_rangeCombo;
    QComboBox*   m_intervalCombo;
    QComboBox*   m_indicatorCombo;
    QSpinBox*    m_periodSpin;
    QPushButton* m_fetchButton;

    CandleSeries m_lastCandles;
    CandleSeries m_allCandles;     // chart data (user-selected range+interval)
    CandleSeries m_historyCandles; // 26y of daily bars for performance panels

    // Queue of [period1, period2] segments for the sequential history fetch chain.
    QVector<QPair<qint64, qint64>> m_historySegments;

    // Collapsible panels
    CollapsiblePanel* m_marketPanel    = nullptr;
    CollapsiblePanel* m_perfSincePanel = nullptr;
    CollapsiblePanel* m_perfYearPanel  = nullptr;

    // Current Market labels
    QLabel* m_mktDate  = nullptr;
    QLabel* m_mktOpen  = nullptr;
    QLabel* m_mktHigh  = nullptr;
    QLabel* m_mktLow   = nullptr;
    QLabel* m_mktClose = nullptr;
    QLabel* m_mktVol   = nullptr;

    // Performance Since labels (1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, 20y, max)
    QLabel* m_perfSince[9] = {};

    // Oldest available price for "max" since-IPO calculation (from quarterly fetch).
    Candle m_maxStartCandle;
    bool   m_maxStartValid = false;

    // Performance by Year body layout (rebuilt on each update)
    QGridLayout* m_yearGrid = nullptr;
};
