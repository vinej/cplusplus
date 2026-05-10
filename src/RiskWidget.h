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

    void computeAndDisplay();
    RiskResult computePortfolioMetrics(const QVector<double>& portRet,
                                       const QVector<double>& spyRet,
                                       double riskFreeAnnual) const;
    QVector<QVector<double>> computeCorrMatrix(
        const QStringList& syms,
        const QMap<QString, QVector<double>>& symReturns) const;
    void rebuildDisplay(const RiskResult& r,
                        const QStringList& symbols,
                        const QVector<QVector<double>>& corrMx);
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

    // Scalar metric value labels
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

    QWidget* m_corrContainer = nullptr;  // rebuilt on each compute
    QWidget* m_resultsWidget = nullptr;  // hidden until first compute

    YahooFinanceClient* m_client = nullptr;

    QVector<PortfolioPosition>         m_positions;
    QMap<QString, QMap<QDate, double>> m_prices;   // symbol → date → adjClose
    int                                m_pending = 0;
};
