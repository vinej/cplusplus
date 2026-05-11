#pragma once

#include "Candle.h"
#include "PortfolioDb.h"

#include <QMap>
#include <QWidget>
#include <limits>

class QColor;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class YahooFinanceClient;

class RiskWidget : public QWidget {
    Q_OBJECT
public:
    explicit RiskWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

public slots:
    void loadPortfolios();

private slots:
    void onFetch();
    void onFetchFinished(const QString& symbol, const QString& tag,
                         const CandleSeries& candles, const QString& name);
    void onFetchFailed(const QString& symbol, const QString& tag,
                       const QString& message);

private:
    struct RiskResult {
        double annualReturn = std::numeric_limits<double>::quiet_NaN();
        double annualVol    = std::numeric_limits<double>::quiet_NaN();
        double sharpe       = std::numeric_limits<double>::quiet_NaN();
        double var95        = std::numeric_limits<double>::quiet_NaN();
        double cvar95       = std::numeric_limits<double>::quiet_NaN();
        double stdDev       = std::numeric_limits<double>::quiet_NaN();
        double kurtosis     = std::numeric_limits<double>::quiet_NaN();
        double skewness     = std::numeric_limits<double>::quiet_NaN();
        double beta         = std::numeric_limits<double>::quiet_NaN();
        double alpha        = std::numeric_limits<double>::quiet_NaN();
    };

    struct SymbolRiskContrib {
        QString symbol;
        double  weight        = 0.0;   // %
        double  annualVol     = 0.0;   // % annualised
        double  riskPct       = 0.0;   // % contribution to portfolio variance
        double  marginalSigma = 0.0;   // marginal contribution to annual portfolio vol, %
    };

    void computeAndDisplay();
    RiskResult computePortfolioMetrics(const QVector<double>& portRet,
                                       const QVector<double>& portRetAligned,
                                       const QVector<double>& spyRet,
                                       double riskFreeAnnual,
                                       double calendarYears) const;
    QVector<QVector<double>> computeCorrMatrix(
        const QStringList& syms,
        const QMap<QString, QVector<double>>& symReturns) const;
    void rebuildDisplay(const RiskResult& r,
                        const QStringList& symbols,
                        const QVector<QVector<double>>& corrMx,
                        const QVector<SymbolRiskContrib>& riskContribs);
    QMap<QString, double> symbolWeights() const;

    static double sMean(const QVector<double>& v);
    static double sVariance(const QVector<double>& v);
    static double sCovariance(const QVector<double>& a, const QVector<double>& b);
    static QColor corrColor(double v);

    // Controls
    QComboBox*      m_portfolioCombo  = nullptr;
    QComboBox*      m_lookbackCombo   = nullptr;
    QDoubleSpinBox* m_riskFreeSpinBox = nullptr;
    QPushButton*    m_fetchBtn        = nullptr;

    // Metric value labels
    QLabel* m_annualReturnLbl = nullptr;
    QLabel* m_annualVolLbl    = nullptr;
    QLabel* m_sharpeLbl       = nullptr;
    QLabel* m_stdDevLbl       = nullptr;
    QLabel* m_varLbl          = nullptr;
    QLabel* m_cvarLbl         = nullptr;
    QLabel* m_kurtosisLbl     = nullptr;
    QLabel* m_skewnessLbl     = nullptr;
    QLabel* m_betaLbl         = nullptr;
    QLabel* m_alphaLbl        = nullptr;

    // Metric interpretation labels (col 3 of the 3-column grid)
    QLabel* m_annualReturnIntLbl = nullptr;
    QLabel* m_annualVolIntLbl    = nullptr;
    QLabel* m_sharpeIntLbl       = nullptr;
    QLabel* m_stdDevIntLbl       = nullptr;
    QLabel* m_varIntLbl          = nullptr;
    QLabel* m_cvarIntLbl         = nullptr;
    QLabel* m_kurtosisIntLbl     = nullptr;
    QLabel* m_skewnessIntLbl     = nullptr;
    QLabel* m_betaIntLbl         = nullptr;
    QLabel* m_alphaIntLbl        = nullptr;

    QLabel*  m_corrSummaryLbl    = nullptr;   // diversification verdict above the matrix
    QWidget* m_corrContainer     = nullptr;   // grid rebuilt on each compute
    QWidget* m_riskContribWidget = nullptr;   // grid rebuilt on each compute
    QWidget* m_resultsWidget     = nullptr;   // hidden until first compute

    YahooFinanceClient* m_client = nullptr;

    QVector<PortfolioPosition>         m_positions;
    QMap<QString, QMap<QDate, double>> m_prices;
    int                                m_pending = 0;
};
