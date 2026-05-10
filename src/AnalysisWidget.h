#pragma once

#include "Candle.h"

#include <QWidget>

class CandleChart;
class MarketPanel;
class PerfSincePanel;
class PerfYearPanel;
class RsiChart;
class SymbolPicker;
class QComboBox;
class QScrollArea;
class QSpinBox;
class QTabWidget;
class YahooFinanceClient;

class AnalysisWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisWidget(QWidget* parent = nullptr);

    bool eventFilter(QObject* obj, QEvent* e) override;

signals:
    void statusMessage(const QString& msg);

private slots:
    void onFetchClicked();
    void onFetchFinished(const QString& symbol, const QString& tag, const CandleSeries& candles, const QString& name);
    void onFetchFailed  (const QString& symbol, const QString& tag, const QString& message);

private:
    void applyIndicator();
    void updatePanels();
    void fetchChartOnly();

    YahooFinanceClient* m_client;
    CandleChart*        m_chart;
    RsiChart*           m_rsiChart;
    QScrollArea*        m_scrollArea;

    SymbolPicker* m_symbolPicker;
    QComboBox*    m_indicatorCombo;
    QSpinBox*     m_periodSpin;

    CandleSeries m_lastCandles;
    CandleSeries m_historyCandles;
    CandleSeries m_maxCandles;
    bool         m_maxCandlesValid = false;

    MarketPanel*    m_marketPanel    = nullptr;
    PerfSincePanel* m_perfSincePanel = nullptr;
    PerfYearPanel*  m_perfYearPanel  = nullptr;
};
