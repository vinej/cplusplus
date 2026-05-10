#pragma once

#include "Candle.h"
#include "PerformanceChart.h"
#include "PortfolioDb.h"

#include <QMap>
#include <QWidget>

class MarketPanel;
class PerfSincePanel;
class PerfYearPanel;
class QComboBox;
class QPushButton;
class YahooFinanceClient;

class PortfolioAnalysisWidget : public QWidget {
    Q_OBJECT
public:
    explicit PortfolioAnalysisWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

public slots:
    void loadPortfolios();

private slots:
    void onFetch();
    void onFetchFinished(const QString& symbol, const QString& tag,
                         const CandleSeries& candles, const QString& name);
    void onFetchFailed (const QString& symbol, const QString& tag,
                        const QString& message);

private:
    QDate startDate() const;
    void  renderChart();
    void  renderPanels();
    QMap<QString,double> symbolWeights() const;

    // Controls
    QComboBox*   m_portfolioCombo;
    QComboBox*   m_rangeCombo;
    QComboBox*   m_intervalCombo;
    QPushButton* m_fetchBtn;

    // Charts & panels
    PerformanceChart* m_chart;
    MarketPanel*      m_marketPanel;
    PerfSincePanel*   m_perfSincePanel;
    PerfYearPanel*    m_perfYearPanel;

    // Network
    YahooFinanceClient* m_client;

    // State
    QVector<PortfolioPosition>           m_positions;
    QMap<QString, CandleSeries>          m_symbolCandles;   // chart-range data
    QMap<QString, QMap<QDate, double>>   m_historyPrices;   // 27y daily close by date
    int                                  m_pendingChart   = 0;
    int                                  m_pendingHistory = 0;
};
