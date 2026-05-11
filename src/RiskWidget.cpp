#include "RiskWidget.h"

#include "PortfolioDb.h"
#include "YahooFinanceClient.h"

#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

// ── static math helpers ───────────────────────────────────────────────────────

double RiskWidget::sMean(const QVector<double>& v)
{
    if (v.isEmpty()) return 0.0;
    double s = 0.0;
    for (double x : v) s += x;
    return s / v.size();
}

double RiskWidget::sVariance(const QVector<double>& v)
{
    if (v.size() < 2) return 0.0;
    const double m = sMean(v);
    double s = 0.0;
    for (double x : v) { const double d = x - m; s += d * d; }
    return s / (v.size() - 1);
}

double RiskWidget::sCovariance(const QVector<double>& a, const QVector<double>& b)
{
    const int n = qMin(a.size(), b.size());
    if (n < 2) return 0.0;
    const double ma = sMean(a), mb = sMean(b);
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (a[i] - ma) * (b[i] - mb);
    return s / (n - 1);
}

// Blue-white-red heatmap: +1 → #1565C0, 0 → white, −1 → #C62828
QColor RiskWidget::corrColor(double v)
{
    v = qBound(-1.0, v, 1.0);
    if (v >= 0.0)
        return QColor(static_cast<int>(255 - v * 234),
                      static_cast<int>(255 - v * 154),
                      static_cast<int>(255 - v *  63));
    const double a = -v;
    return QColor(static_cast<int>(255 - a *  57),
                  static_cast<int>(255 - a * 215),
                  static_cast<int>(255 - a * 215));
}

// ── constructor ───────────────────────────────────────────────────────────────

RiskWidget::RiskWidget(QWidget* parent)
    : QWidget(parent)
    , m_client(new YahooFinanceClient(this))
{
    // ── Control row ───────────────────────────────────────────────────────────
    m_portfolioCombo = new QComboBox(this);

    m_lookbackCombo = new QComboBox(this);
    m_lookbackCombo->addItems({"1y", "2y", "5y"});
    m_lookbackCombo->setCurrentIndex(1);

    m_riskFreeSpinBox = new QDoubleSpinBox(this);
    m_riskFreeSpinBox->setRange(0.0, 20.0);
    m_riskFreeSpinBox->setDecimals(2);
    m_riskFreeSpinBox->setSingleStep(0.10);
    m_riskFreeSpinBox->setValue(4.30);
    m_riskFreeSpinBox->setSuffix(" %");
    m_riskFreeSpinBox->setFixedWidth(95);
    m_riskFreeSpinBox->setToolTip("Annual risk-free rate used for Sharpe and Alpha");

    m_fetchBtn = new QPushButton("Fetch", this);

    auto* controlRow = new QHBoxLayout();
    controlRow->addWidget(new QLabel("Portfolio:", this));
    controlRow->addWidget(m_portfolioCombo);
    controlRow->addSpacing(10);
    controlRow->addWidget(new QLabel("Lookback:", this));
    controlRow->addWidget(m_lookbackCombo);
    controlRow->addSpacing(10);
    controlRow->addWidget(new QLabel("Risk Free:", this));
    controlRow->addWidget(m_riskFreeSpinBox);
    controlRow->addSpacing(10);
    controlRow->addWidget(m_fetchBtn);
    controlRow->addStretch(1);

    // ── Label factories ───────────────────────────────────────────────────────
    auto mkName = [this](const QString& t) {
        auto* l = new QLabel(t + ":", this);
        l->setStyleSheet("font-weight: bold; color: #AAAAAA;");
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };
    auto mkVal = [this]() {
        auto* l = new QLabel("—", this);
        l->setMinimumWidth(90);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        l->setStyleSheet("font-size: 14px;");
        return l;
    };
    auto mkInterp = [this]() {
        auto* l = new QLabel("—", this);
        l->setMinimumWidth(400);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        l->setStyleSheet("font-size: 13px; color: #888888;");
        return l;
    };

    m_annualReturnLbl = mkVal();  m_annualReturnIntLbl = mkInterp();
    m_annualVolLbl    = mkVal();  m_annualVolIntLbl    = mkInterp();
    m_sharpeLbl       = mkVal();  m_sharpeIntLbl       = mkInterp();
    m_stdDevLbl       = mkVal();  m_stdDevIntLbl       = mkInterp();
    m_varLbl          = mkVal();  m_varIntLbl          = mkInterp();
    m_cvarLbl         = mkVal();  m_cvarIntLbl         = mkInterp();
    m_kurtosisLbl     = mkVal();  m_kurtosisIntLbl     = mkInterp();
    m_skewnessLbl     = mkVal();  m_skewnessIntLbl     = mkInterp();
    m_betaLbl         = mkVal();  m_betaIntLbl         = mkInterp();
    m_alphaLbl        = mkVal();  m_alphaIntLbl        = mkInterp();

    // ── Metrics group: 3-column grid (name | value | interpretation) ──────────
    auto* metricsBox  = new QGroupBox("Portfolio Risk Metrics", this);
    metricsBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* metricsGrid = new QGridLayout(metricsBox);
    metricsGrid->setHorizontalSpacing(12);
    metricsGrid->setVerticalSpacing(8);

    const auto addRow = [&](int row, const QString& name, QLabel* val, QLabel* interp) {
        metricsGrid->addWidget(mkName(name), row, 0);
        metricsGrid->addWidget(val,          row, 1);
        metricsGrid->addWidget(interp,       row, 2);
    };
    addRow(0, "Annual Return",     m_annualReturnLbl, m_annualReturnIntLbl);
    addRow(1, "Annual Volatility", m_annualVolLbl,    m_annualVolIntLbl);
    addRow(2, "Sharpe Ratio",      m_sharpeLbl,       m_sharpeIntLbl);
    addRow(3, "Std Dev (daily)",   m_stdDevLbl,       m_stdDevIntLbl);
    addRow(4, "VaR (95%, 1d)",     m_varLbl,          m_varIntLbl);
    addRow(5, "CVaR (95%, 1d)",    m_cvarLbl,         m_cvarIntLbl);
    addRow(6, "Kurtosis",          m_kurtosisLbl,     m_kurtosisIntLbl);
    addRow(7, "Skewness",          m_skewnessLbl,     m_skewnessIntLbl);
    addRow(8, "Beta (vs SPY)",     m_betaLbl,         m_betaIntLbl);
    addRow(9, "Alpha (vs SPY)",    m_alphaLbl,        m_alphaIntLbl);

    // ── Correlation matrix group ──────────────────────────────────────────────
    m_corrContainer  = new QWidget(this);
    m_corrSummaryLbl = new QLabel("", this);
    m_corrSummaryLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_corrSummaryLbl->setStyleSheet("font-size: 13px; padding: 2px 0 6px 0;");
    m_corrSummaryLbl->setTextFormat(Qt::RichText);

    auto* corrBox    = new QGroupBox("Correlation Matrix  (incl. SPY)", this);
    corrBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* corrBoxLay = new QVBoxLayout(corrBox);
    corrBoxLay->addWidget(m_corrSummaryLbl);
    corrBoxLay->addWidget(m_corrContainer);
    corrBoxLay->addStretch(1);

    // ── Risk contribution by symbol ───────────────────────────────────────────
    m_riskContribWidget = new QWidget(this);

    auto* rcBox    = new QGroupBox("Risk Contribution by Symbol", this);
    rcBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* rcBoxLay = new QVBoxLayout(rcBox);
    rcBoxLay->addWidget(m_riskContribWidget);
    rcBoxLay->addStretch(1);

    // ── Results layout ────────────────────────────────────────────────────────
    auto* metricsRow = new QHBoxLayout();
    metricsRow->setContentsMargins(0, 0, 0, 0);
    metricsRow->addWidget(metricsBox, 0);
    metricsRow->addStretch(1);

    auto* corrRow = new QHBoxLayout();
    corrRow->setContentsMargins(0, 0, 0, 0);
    corrRow->addWidget(corrBox, 0);
    corrRow->addStretch(1);

    auto* rcRow = new QHBoxLayout();
    rcRow->setContentsMargins(0, 0, 0, 0);
    rcRow->addWidget(rcBox, 0);
    rcRow->addStretch(1);

    m_resultsWidget = new QWidget(this);
    auto* resLay = new QVBoxLayout(m_resultsWidget);
    resLay->setContentsMargins(0, 0, 0, 0);
    resLay->setSpacing(12);
    resLay->addLayout(metricsRow);
    resLay->addLayout(corrRow);
    resLay->addLayout(rcRow);
    resLay->addStretch(1);
    m_resultsWidget->setVisible(false);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(m_resultsWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // ── Root layout ───────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->addLayout(controlRow);
    root->addWidget(scrollArea, 1);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_fetchBtn, &QPushButton::clicked, this, &RiskWidget::onFetch);

    connect(m_lookbackCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_pending == 0 && !m_prices.isEmpty()) computeAndDisplay();
    });

    connect(m_client, &YahooFinanceClient::finished, this, &RiskWidget::onFetchFinished);
    connect(m_client, &YahooFinanceClient::failed,   this, &RiskWidget::onFetchFailed);

    PortfolioDb::instance().open();
    loadPortfolios();
}

// ── portfolio list ────────────────────────────────────────────────────────────

void RiskWidget::loadPortfolios()
{
    const int prevId = m_portfolioCombo->currentData().toInt();
    m_portfolioCombo->blockSignals(true);
    m_portfolioCombo->clear();
    m_portfolioCombo->addItem("— select portfolio —", 0);
    for (const auto& [id, name] : PortfolioDb::instance().portfolios())
        m_portfolioCombo->addItem(name, id);
    const int idx = m_portfolioCombo->findData(prevId);
    m_portfolioCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_portfolioCombo->blockSignals(false);
}

// ── fetch ─────────────────────────────────────────────────────────────────────

void RiskWidget::onFetch()
{
    const int portfolioId = m_portfolioCombo->currentData().toInt();
    if (portfolioId <= 0) return;

    m_positions = PortfolioDb::instance().positions(portfolioId);
    if (m_positions.isEmpty()) {
        emit statusMessage("Portfolio has no positions.");
        return;
    }

    QStringList portSymbols;
    for (const auto& pos : m_positions)
        if (!portSymbols.contains(pos.symbol)) portSymbols.append(pos.symbol);

    // Always include SPY as benchmark for Beta / Alpha
    QStringList allSymbols = portSymbols;
    if (!allSymbols.contains("SPY")) allSymbols.append("SPY");

    m_prices.clear();
    m_pending = allSymbols.size();
    m_fetchBtn->setEnabled(false);
    emit statusMessage(
        QString("Fetching %1 symbols for risk analysis...").arg(allSymbols.size()));

    // Always fetch 5 years + buffer so switching the lookback doesn't require a re-fetch.
    const QDate endDate   = QDate::currentDate();
    const QDate startDate = endDate.addYears(-5).addDays(-30);

    for (const QString& sym : allSymbols)
        m_client->fetch(sym, "1d", startDate, endDate, "risk:" + sym);
}

void RiskWidget::onFetchFinished(const QString& /*symbol*/, const QString& tag,
                                  const CandleSeries& candles, const QString& /*name*/)
{
    if (!tag.startsWith("risk:")) return;
    const QString sym = tag.mid(5);

    auto& pm = m_prices[sym];
    for (const Candle& c : candles) {
        const double p = c.adjClose > 0.0 ? c.adjClose : c.close;
        if (p > 0.0) pm[c.timestamp.date()] = p;
    }

    if (--m_pending == 0) {
        m_fetchBtn->setEnabled(true);
        computeAndDisplay();
    }
}

void RiskWidget::onFetchFailed(const QString& /*symbol*/, const QString& tag,
                                const QString& message)
{
    if (!tag.startsWith("risk:")) return;
    emit statusMessage(QString("Failed: %1 — %2").arg(tag.mid(5), message));
    if (--m_pending == 0) {
        m_fetchBtn->setEnabled(true);
        if (!m_prices.isEmpty()) computeAndDisplay();
    }
}

// ── computation ───────────────────────────────────────────────────────────────

void RiskWidget::computeAndDisplay()
{
    const QString lbStr     = m_lookbackCombo->currentText();
    const int lookbackYears = lbStr == "1y" ? 1 : lbStr == "2y" ? 2 : 5;
    const QDate endDate     = QDate::currentDate();
    const QDate startDate   = endDate.addYears(-lookbackYears);
    const double calendarYears = startDate.daysTo(endDate) / 365.25;

    QStringList portSymbols;
    for (const auto& pos : m_positions)
        if (!portSymbols.contains(pos.symbol)) portSymbols.append(pos.symbol);

    QStringList allSymbols = portSymbols;
    if (!allSymbols.contains("SPY")) allSymbols.append("SPY");

    for (const QString& sym : allSymbols) {
        if (!m_prices.contains(sym) || m_prices[sym].isEmpty()) {
            emit statusMessage(
                QString("No price data for %1 — please refetch.").arg(sym));
            return;
        }
    }

    // ── Date intersection helper ──────────────────────────────────────────────
    auto intersectDates = [&](const QStringList& syms) -> QList<QDate> {
        QSet<QDate> common;
        bool first = true;
        for (const QString& sym : syms) {
            const auto& pm = m_prices[sym];
            QSet<QDate> d;
            for (auto it = pm.lowerBound(startDate); it != pm.end() && it.key() <= endDate; ++it)
                d.insert(it.key());
            if (first) { common = d; first = false; }
            else         common &= d;
        }
        QList<QDate> sorted = common.values();
        std::sort(sorted.begin(), sorted.end());
        return sorted;
    };

    // Portfolio symbols only — larger date set, used for all portfolio risk metrics.
    // Excluding SPY prevents US/Canadian holiday mismatches from shrinking n.
    const QList<QDate> portDates = intersectDates(portSymbols);
    // All symbols incl. SPY — used for Beta/Alpha and correlation matrix.
    const QList<QDate> allDates  = intersectDates(allSymbols);

    const int nPort = portDates.size();
    const int nAll  = allDates.size();

    if (nPort < 20) {
        emit statusMessage(
            QString("Only %1 common portfolio trading days — need at least 20.").arg(nPort));
        return;
    }

    // ── Daily return series builder ───────────────────────────────────────────
    auto buildReturns = [&](const QString& sym, const QList<QDate>& dates) -> QVector<double> {
        const auto& pm = m_prices[sym];
        QVector<double> ret;
        ret.reserve(dates.size());
        for (int i = 0; i < dates.size(); ++i) {
            const double p1 = pm.value(dates[i]);
            double p0;
            if (i == 0) {
                auto it = pm.lowerBound(dates[0]);
                p0 = (it != pm.begin()) ? (--it).value() : p1;
            } else {
                p0 = pm.value(dates[i - 1]);
            }
            ret.append(p0 > 0.0 ? (p1 / p0 - 1.0) : 0.0);
        }
        return ret;
    };

    // Portfolio symbols on portDates (for portfolio risk metrics)
    QMap<QString, QVector<double>> portSymReturns;
    for (const QString& sym : portSymbols)
        portSymReturns[sym] = buildReturns(sym, portDates);

    // All symbols on allDates (for Beta/Alpha and correlation matrix)
    QMap<QString, QVector<double>> allSymReturns;
    for (const QString& sym : allSymbols)
        allSymReturns[sym] = buildReturns(sym, allDates);

    // ── Portfolio weighted returns on portDates (for risk metrics) ────────────
    const QMap<QString, double> weights = symbolWeights();
    QVector<double> portReturns(nPort, 0.0);
    for (const QString& sym : portSymbols) {
        if (!portSymReturns.contains(sym) || !weights.contains(sym)) continue;
        const double w   = weights[sym];
        const auto&  ret = portSymReturns[sym];
        for (int i = 0; i < nPort; ++i) portReturns[i] += w * ret[i];
    }

    // ── Portfolio weighted returns on allDates (aligned with SPY for Beta/Alpha)
    QVector<double> portReturnsAligned(nAll, 0.0);
    for (const QString& sym : portSymbols) {
        if (!allSymReturns.contains(sym) || !weights.contains(sym)) continue;
        const double w   = weights[sym];
        const auto&  ret = allSymReturns[sym];
        for (int i = 0; i < nAll; ++i) portReturnsAligned[i] += w * ret[i];
    }

    // ── Per-symbol risk contributions ─────────────────────────────────────────
    // Covariance matrix on portfolio-only dates, then Σw = Cov × w.
    const int nSyms = portSymbols.size();
    QVector<QVector<double>> covMx(nSyms, QVector<double>(nSyms, 0.0));
    for (int i = 0; i < nSyms; ++i) {
        const auto& a = portSymReturns[portSymbols[i]];
        covMx[i][i] = sVariance(a);
        for (int j = i + 1; j < nSyms; ++j) {
            const double c = sCovariance(a, portSymReturns[portSymbols[j]]);
            covMx[i][j] = covMx[j][i] = c;
        }
    }
    QVector<double> wVec(nSyms);
    for (int i = 0; i < nSyms; ++i) wVec[i] = weights.value(portSymbols[i], 0.0);

    QVector<double> sigmaW(nSyms, 0.0);
    for (int i = 0; i < nSyms; ++i)
        for (int j = 0; j < nSyms; ++j)
            sigmaW[i] += covMx[i][j] * wVec[j];

    double portVarDaily = 0.0;
    for (int i = 0; i < nSyms; ++i) portVarDaily += wVec[i] * sigmaW[i];
    const double portSigmaDaily = std::sqrt(portVarDaily);

    QVector<SymbolRiskContrib> riskContribs(nSyms);
    for (int i = 0; i < nSyms; ++i) {
        auto& rc = riskContribs[i];
        rc.symbol        = portSymbols[i];
        rc.weight        = wVec[i] * 100.0;
        rc.annualVol     = std::sqrt(covMx[i][i]) * std::sqrt(252.0) * 100.0;
        rc.riskPct       = portVarDaily > 1e-14
            ? wVec[i] * sigmaW[i] / portVarDaily * 100.0 : 0.0;
        rc.marginalSigma = portSigmaDaily > 1e-12
            ? sigmaW[i] / portSigmaDaily * std::sqrt(252.0) * 100.0 : 0.0;
    }

    // ── Scalar metrics ────────────────────────────────────────────────────────
    const double riskFreeAnnual = m_riskFreeSpinBox->value() / 100.0;
    const RiskResult result = computePortfolioMetrics(
        portReturns, portReturnsAligned,
        allSymReturns.value("SPY"),
        riskFreeAnnual, calendarYears);

    // ── Correlation matrix (all symbols incl. SPY, aligned dates) ────────────
    const QVector<QVector<double>> corrMx = computeCorrMatrix(allSymbols, allSymReturns);

    rebuildDisplay(result, allSymbols, corrMx, riskContribs);

    emit statusMessage(
        QString("Risk computed — %1 port days | %2 aligned days | %3 symbol(s) | lookback: %4")
            .arg(nPort).arg(nAll).arg(portSymbols.size()).arg(lbStr));
}

RiskWidget::RiskResult RiskWidget::computePortfolioMetrics(
    const QVector<double>& portRet,
    const QVector<double>& portRetAligned,
    const QVector<double>& spyRet,
    double riskFreeAnnual,
    double calendarYears) const
{
    RiskResult r;
    const int n = portRet.size();
    if (n < 5) return r;

    // Annual return: CAGR using actual calendar years.
    // Using 252/n would over-annualize whenever the date intersection is smaller
    // than expected (e.g. cross-border ETFs on different holiday schedules).
    double totalRet = 1.0;
    for (double x : portRet) totalRet *= (1.0 + x);
    r.annualReturn = std::pow(totalRet, 1.0 / calendarYears) - 1.0;

    // Daily std dev → annualised volatility (252 trading days/year convention)
    r.stdDev    = std::sqrt(sVariance(portRet));
    r.annualVol = r.stdDev * std::sqrt(252.0);

    // Sharpe ratio
    r.sharpe = r.annualVol > 1e-12
        ? (r.annualReturn - riskFreeAnnual) / r.annualVol
        : 0.0;

    // VaR (95% confidence, 1-day): return at the 5th percentile of sorted returns
    QVector<double> sorted = portRet;
    std::sort(sorted.begin(), sorted.end());
    const int varIdx = std::max(0, static_cast<int>(n * 0.05) - 1);
    r.var95 = sorted[varIdx];

    // CVaR = mean of all returns strictly worse than VaR
    double cvarSum = 0.0;
    const int cnt  = varIdx + 1;
    for (int i = 0; i < cnt; ++i) cvarSum += sorted[i];
    r.cvar95 = cvarSum / cnt;

    // Kurtosis (excess) and skewness — population (÷n) moments
    if (r.stdDev > 1e-12) {
        const double mu = sMean(portRet);
        double k4 = 0.0, k3 = 0.0;
        for (double x : portRet) {
            const double z  = (x - mu) / r.stdDev;
            const double z2 = z * z;
            k4 += z2 * z2;
            k3 += z2 * z;
        }
        r.kurtosis = k4 / n - 3.0;
        r.skewness = k3 / n;
    }

    // Beta and Alpha vs SPY — use portRetAligned which shares dates with spyRet
    const int ns = qMin(portRetAligned.size(), spyRet.size());
    if (ns >= 5) {
        const QVector<double> pSub(portRetAligned.cbegin(), portRetAligned.cbegin() + ns);
        const QVector<double> sSub(spyRet.cbegin(),         spyRet.cbegin()         + ns);

        const double spyVar = sVariance(sSub);
        r.beta = spyVar > 1e-12 ? sCovariance(pSub, sSub) / spyVar : 0.0;

        double spyTotal = 1.0;
        for (double x : sSub) spyTotal *= (1.0 + x);
        const double spyAnnual = std::pow(spyTotal, 1.0 / calendarYears) - 1.0;

        // CAPM Alpha (annualised)
        r.alpha = r.annualReturn - (riskFreeAnnual + r.beta * (spyAnnual - riskFreeAnnual));
    }

    return r;
}

QVector<QVector<double>> RiskWidget::computeCorrMatrix(
    const QStringList& syms,
    const QMap<QString, QVector<double>>& symReturns) const
{
    const int n = syms.size();
    QVector<QVector<double>> mx(n, QVector<double>(n, 0.0));

    for (int i = 0; i < n; ++i) {
        mx[i][i] = 1.0;
        const auto&  a  = symReturns.value(syms[i]);
        const double sa = std::sqrt(sVariance(a));
        for (int j = i + 1; j < n; ++j) {
            const auto&  b  = symReturns.value(syms[j]);
            const double sb = std::sqrt(sVariance(b));
            const double corr = (sa > 1e-12 && sb > 1e-12)
                ? qBound(-1.0, sCovariance(a, b) / (sa * sb), 1.0)
                : 0.0;
            mx[i][j] = mx[j][i] = corr;
        }
    }
    return mx;
}

// ── display ───────────────────────────────────────────────────────────────────

void RiskWidget::rebuildDisplay(const RiskResult& r,
                                 const QStringList& symbols,
                                 const QVector<QVector<double>>& corrMx,
                                 const QVector<SymbolRiskContrib>& riskContribs)
{
    // ── Formatting helpers ────────────────────────────────────────────────────
    auto fmtVal = [](double v, int dp, bool pct) -> QString {
        if (!std::isfinite(v)) return "N/A";
        const double disp = pct ? v * 100.0 : v;
        return (v > 0.0 ? "+" : "") + QString::number(disp, 'f', dp) + (pct ? "%" : "");
    };

    enum Rating { Good, Neutral, Bad };
    auto rateColor = [](Rating rt) -> const char* {
        switch (rt) {
        case Good:    return "#4CAF50";
        case Neutral: return "#2196F3";
        case Bad:     return "#F44336";
        }
        return "#888888";
    };

    // Sets value label (colored by sign when colorizeVal=true) and interpretation label.
    auto setRow = [&](QLabel* valLbl, QLabel* intLbl,
                      double v, int dp, bool pct, bool colorizeVal,
                      const QString& text, Rating rating) {
        valLbl->setText(fmtVal(v, dp, pct));
        if (colorizeVal && std::isfinite(v) && v != 0.0) {
            valLbl->setStyleSheet(v > 0.0
                ? "color: #4CAF50; font-size: 14px; font-weight: bold;"
                : "color: #F44336; font-size: 14px; font-weight: bold;");
        } else {
            valLbl->setStyleSheet("font-size: 14px;");
        }
        if (std::isfinite(v)) {
            intLbl->setText(text);
            intLbl->setStyleSheet(
                QString("font-size: 13px; color: %1;").arg(rateColor(rating)));
        } else {
            intLbl->setText("—");
            intLbl->setStyleSheet("font-size: 13px; color: #888888;");
        }
    };

    // ── Annual Return ─────────────────────────────────────────────────────────
    {
        const double v = r.annualReturn;
        QString t; Rating rt;
        if      (v >= 0.15) { t = "Strong — above long-term equity market returns";            rt = Good; }
        else if (v >= 0.08) { t = "Moderate — in line with historical equity returns";          rt = Neutral; }
        else if (v >= 0.00) { t = "Below average — underperforming typical market benchmarks";  rt = Neutral; }
        else                { t = "Negative — portfolio is losing value";                        rt = Bad; }
        setRow(m_annualReturnLbl, m_annualReturnIntLbl, v, 2, true, true, t, rt);
    }
    // ── Annual Volatility ─────────────────────────────────────────────────────
    {
        const double v = r.annualVol;
        QString t; Rating rt;
        if      (v <= 0.08) { t = "Low — bond-like stability, well-controlled risk";             rt = Good; }
        else if (v <= 0.15) { t = "Moderate — typical for a diversified equity portfolio";       rt = Neutral; }
        else if (v <= 0.25) { t = "Elevated — concentrated or high-beta positions";              rt = Bad; }
        else                { t = "Very high — aggressive or speculative portfolio";              rt = Bad; }
        setRow(m_annualVolLbl, m_annualVolIntLbl, v, 2, true, false, t, rt);
    }
    // ── Sharpe Ratio ──────────────────────────────────────────────────────────
    {
        const double v = r.sharpe;
        QString t; Rating rt;
        if      (v >= 2.0)  { t = "Excellent — exceptional risk-adjusted return";                rt = Good; }
        else if (v >= 1.0)  { t = "Strong — well compensated for risk taken";                    rt = Good; }
        else if (v >= 0.5)  { t = "Acceptable — moderate risk-adjusted return";                  rt = Neutral; }
        else if (v >= 0.0)  { t = "Weak — returns barely justify the risk";                      rt = Neutral; }
        else                { t = "Negative — returns fail to cover the risk-free rate";          rt = Bad; }
        setRow(m_sharpeLbl, m_sharpeIntLbl, v, 3, false, true, t, rt);
    }
    // ── Std Dev (daily) ───────────────────────────────────────────────────────
    {
        const double v = r.stdDev;
        QString t; Rating rt;
        if      (v <= 0.005) { t = "Low — minimal daily price fluctuation";                      rt = Good; }
        else if (v <= 0.010) { t = "Normal — typical daily moves for equities";                  rt = Neutral; }
        else                  { t = "High — significant intraday swings";                         rt = Bad; }
        setRow(m_stdDevLbl, m_stdDevIntLbl, v, 4, true, false, t, rt);
    }
    // ── VaR (95%, 1d) ─────────────────────────────────────────────────────────
    {
        const double v = r.var95;
        QString t; Rating rt;
        if      (v >= -0.010) { t = "Low tail risk — rarely exceeds 1% loss in a day";           rt = Good; }
        else if (v >= -0.020) { t = "Moderate — expected 1-in-20 daily loss for equity portfolios"; rt = Neutral; }
        else                   { t = "High tail risk — large 1-in-20 daily loss possible";        rt = Bad; }
        setRow(m_varLbl, m_varIntLbl, v, 2, true, false, t, rt);
    }
    // ── CVaR (95%, 1d) ────────────────────────────────────────────────────────
    {
        const double v = r.cvar95;
        QString t; Rating rt;
        if      (v >= -0.015) { t = "Low expected shortfall in worst 5% of days";                rt = Good; }
        else if (v >= -0.030) { t = "Moderate expected tail loss — manageable downside";          rt = Neutral; }
        else                   { t = "Severe — expected loss in worst days is large";             rt = Bad; }
        setRow(m_cvarLbl, m_cvarIntLbl, v, 2, true, false, t, rt);
    }
    // ── Kurtosis ──────────────────────────────────────────────────────────────
    {
        const double v = r.kurtosis;
        QString t; Rating rt;
        if      (v < 0.0)  { t = "Platykurtic — thinner tails than normal distribution";         rt = Neutral; }
        else if (v <= 2.0) { t = "Near normal — typical tail behavior for equities";              rt = Neutral; }
        else if (v <= 5.0) { t = "Fat tails — occasional large moves expected";                   rt = Neutral; }
        else               { t = "Heavy tails — high probability of extreme daily events";        rt = Bad; }
        setRow(m_kurtosisLbl, m_kurtosisIntLbl, v, 3, false, false, t, rt);
    }
    // ── Skewness ──────────────────────────────────────────────────────────────
    {
        const double v = r.skewness;
        QString t; Rating rt;
        if      (v >= 0.5)  { t = "Positive skew — gains tend to outpace losses in extremes";    rt = Good; }
        else if (v >= -0.5) { t = "Near symmetric — balanced return distribution";                rt = Neutral; }
        else                { t = "Negative skew — large losses outweigh frequent small gains";   rt = Bad; }
        setRow(m_skewnessLbl, m_skewnessIntLbl, v, 3, false, false, t, rt);
    }
    // ── Beta ──────────────────────────────────────────────────────────────────
    {
        const double v = r.beta;
        QString t; Rating rt;
        if      (v <= 0.7)  { t = "Defensive — low market exposure, dampens market swings";      rt = Good; }
        else if (v <= 1.1)  { t = "Market-neutral — closely tracks the broad market";             rt = Neutral; }
        else                { t = "Aggressive — amplifies market moves, higher systematic risk";  rt = Bad; }
        setRow(m_betaLbl, m_betaIntLbl, v, 3, false, false, t, rt);
    }
    // ── Alpha ─────────────────────────────────────────────────────────────────
    {
        const double v = r.alpha;
        QString t; Rating rt;
        if      (v >= 0.02)  { t = "Strong alpha — significant outperformance vs CAPM";           rt = Good; }
        else if (v >= 0.0)   { t = "Positive alpha — portfolio adds value above market return";   rt = Good; }
        else if (v >= -0.02) { t = "Near zero alpha — close to market-implied return";            rt = Neutral; }
        else                 { t = "Alpha drag — underperforming market-adjusted return";          rt = Bad; }
        setRow(m_alphaLbl, m_alphaIntLbl, v, 2, true, true, t, rt);
    }

    // ── Correlation summary ───────────────────────────────────────────────────
    {
        double sumCorr = 0.0;
        int numPairs = 0;
        const int n = symbols.size();
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) {
                sumCorr += corrMx[i][j];
                ++numPairs;
            }
        const double avgCorr = numPairs > 0 ? sumCorr / numPairs : 0.0;

        const char* color;  QString label;
        if      (avgCorr <= 0.30) { label = "Strong diversification";                                      color = "#4CAF50"; }
        else if (avgCorr <= 0.50) { label = "Moderate diversification";                                    color = "#2196F3"; }
        else if (avgCorr <= 0.65) { label = "Moderate-high correlation — limited diversification benefit"; color = "#2196F3"; }
        else                      { label = "Weak diversification";                                        color = "#F44336"; }

        m_corrSummaryLbl->setText(
            QString("<span style='color:%1;font-weight:bold'>%2</span>"
                    " — avg pairwise correlation across %3 holdings: <b>%4</b>")
                .arg(color, label).arg(n).arg(avgCorr, 0, 'f', 2));
    }

    // ── Rebuild correlation matrix grid ───────────────────────────────────────
    auto clearWidget = [](QWidget* w) {
        if (QLayout* old = w->layout()) {
            QLayoutItem* item;
            while ((item = old->takeAt(0))) { delete item->widget(); delete item; }
            delete old;
        }
    };
    clearWidget(m_corrContainer);

    const int n = symbols.size();
    auto* grid = new QGridLayout(m_corrContainer);
    grid->setSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    for (int j = 0; j < n; ++j) {
        auto* hdr = new QLabel(symbols[j], m_corrContainer);
        hdr->setAlignment(Qt::AlignCenter);
        hdr->setStyleSheet("font-weight: bold; padding: 2px 6px;");
        grid->addWidget(hdr, 0, j + 1);
    }
    for (int i = 0; i < n; ++i) {
        auto* rhdr = new QLabel(symbols[i], m_corrContainer);
        rhdr->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rhdr->setStyleSheet("font-weight: bold; padding: 2px 6px;");
        grid->addWidget(rhdr, i + 1, 0);

        for (int j = 0; j < n; ++j) {
            const double v  = corrMx[i][j];
            const QColor bg = corrColor(v);
            const bool dark = (bg.red() * 299 + bg.green() * 587 + bg.blue() * 114) < 128000;
            auto* cell = new QLabel(QString::number(v, 'f', 2), m_corrContainer);
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedSize(65, 26);
            cell->setStyleSheet(
                QString("background-color:%1;color:%2;font-size:12px;border-radius:2px;")
                    .arg(bg.name(), dark ? "#FFFFFF" : "#000000"));
            grid->addWidget(cell, i + 1, j + 1);
        }
    }

    // ── Rebuild risk contribution table ──────────────────────────────────────
    clearWidget(m_riskContribWidget);

    if (!riskContribs.isEmpty()) {
        auto* rcGrid = new QGridLayout(m_riskContribWidget);
        rcGrid->setSpacing(1);
        rcGrid->setContentsMargins(0, 0, 0, 0);

        const QStringList hdrs = {"Symbol", "Weight", "Volatility", "% of Risk", "Marginal σ"};
        for (int c = 0; c < hdrs.size(); ++c) {
            auto* hdr = new QLabel(hdrs[c], m_riskContribWidget);
            hdr->setAlignment(Qt::AlignCenter);
            hdr->setFixedHeight(30);
            hdr->setStyleSheet("font-weight: bold; padding: 4px 14px;"
                               "background-color: #E0E0E0; color: #222222;");
            rcGrid->addWidget(hdr, 0, c);
        }

        for (int i = 0; i < riskContribs.size(); ++i) {
            const auto& rc   = riskContribs[i];
            const QString bg = (i % 2 == 0) ? "#FFFFFF" : "#F5F5F5";

            auto mkCell = [&](const QString& text, Qt::Alignment align,
                               const QString& extra = {}) -> QLabel* {
                auto* cell = new QLabel(text, m_riskContribWidget);
                cell->setAlignment(align);
                cell->setFixedHeight(28);
                cell->setStyleSheet(
                    QString("padding: 2px 14px; background-color: %1;"
                            "font-size: 13px; color: #222222; %2").arg(bg, extra));
                return cell;
            };

            rcGrid->addWidget(
                mkCell(rc.symbol, Qt::AlignLeft | Qt::AlignVCenter, "font-weight:bold;"),
                i + 1, 0);
            rcGrid->addWidget(
                mkCell(QString::number(rc.weight, 'f', 1) + "%", Qt::AlignCenter),
                i + 1, 1);
            rcGrid->addWidget(
                mkCell(QString::number(rc.annualVol, 'f', 2) + "%", Qt::AlignCenter),
                i + 1, 2);

            // % of Risk: red if dominant, orange if elevated, green if balanced
            const QString riskColor = rc.riskPct > 40.0 ? "#F44336"
                                    : rc.riskPct > 25.0 ? "#FF9800"
                                    :                      "#4CAF50";
            rcGrid->addWidget(
                mkCell(QString::number(rc.riskPct, 'f', 1) + "%",
                       Qt::AlignCenter,
                       QString("color:%1;font-weight:bold;").arg(riskColor)),
                i + 1, 3);

            rcGrid->addWidget(
                mkCell(QString::number(rc.marginalSigma, 'f', 2) + "%", Qt::AlignCenter),
                i + 1, 4);
        }
    }

    m_resultsWidget->setVisible(true);
}

// ── weights ───────────────────────────────────────────────────────────────────

QMap<QString, double> RiskWidget::symbolWeights() const
{
    QMap<QString, double> weights;
    if (m_positions.isEmpty()) return weights;

    // Collapse to one entry per symbol — updateOriginalWeights sets the same
    // original_weight for every position row of a symbol, so the last value wins.
    // Using += here would multiply the weight by the number of lots per symbol,
    // making all symbols appear equally weighted after normalization.
    QMap<QString, double> rawBySymbol;
    for (const auto& pos : m_positions)
        rawBySymbol[pos.symbol] = pos.originalWeight;   // % e.g. 25.0

    double totalW = 0.0;
    for (double w : rawBySymbol) totalW += w;

    if (totalW > 0.5) {
        for (auto it = rawBySymbol.cbegin(); it != rawBySymbol.cend(); ++it)
            weights[it.key()] = it.value() / 100.0;
    } else {
        const double eq = 1.0 / rawBySymbol.size();
        for (auto it = rawBySymbol.cbegin(); it != rawBySymbol.cend(); ++it)
            weights[it.key()] = eq;
    }

    // Normalise so weights always sum to 1.0
    double sum = 0.0;
    for (double w : weights) sum += w;
    if (sum > 1e-10) for (auto& w : weights) w /= sum;

    return weights;
}
