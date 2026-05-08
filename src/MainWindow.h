#pragma once

#include "Candle.h"

#include <QMainWindow>

class CandleChart;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStatusBar;
class YahooFinanceClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onFetchClicked();
    void onDataReady(const QString& symbol, const CandleSeries& candles);
    void onDataFailed(const QString& symbol, const QString& message);

private:
    void applyIndicator();

    YahooFinanceClient* m_client;
    CandleChart*        m_chart;

    QLineEdit*   m_symbolEdit;
    QComboBox*   m_rangeCombo;
    QComboBox*   m_indicatorCombo;
    QSpinBox*    m_periodSpin;
    QPushButton* m_fetchButton;

    CandleSeries m_lastCandles;
};
