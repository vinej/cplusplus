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
    m_lookbackCombo->setCurrentIndex(1);   // default: 2y

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

    // ── Scalar metrics group ──────────────────────────────────────────────────
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

    m_annualReturnLbl = mkVal();
    m_annualVolLbl    = mkVal();
    m_sharpeLbl       = mkVal();
    m_stdDevLbl       = mkVal();
    m_varLbl          = mkVal();
    m_cvarLbl         = mkVal();
    m_kurtosisLbl     = mkVal();
    m_skewnessLbl     = mkVal();
    m_betaLbl         = mkVal();
    m_alphaLbl        = mkVal();

    // Single name|value column so the panel stays narrow and doesn't stretch.
    auto* metricsBox  = new QGroupBox("Portfolio Risk Metrics", this);
    metricsBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* metricsGrid = new QGridLayout(metricsBox);
    metricsGrid->setHorizontalSpacing(12);
    metricsGrid->setVerticalSpacing(8);

    const auto addRow = [&](int row, const QString& name, QLabel* val) {
        metricsGrid->addWidget(mkName(name), row, 0);
        metricsGrid->addWidget(val,          row, 1);
    };
    addRow(0, "Annual Return",     m_annualReturnLbl);
    addRow(1, "Annual Volatility", m_annualVolLbl);
    addRow(2, "Sharpe Ratio",      m_sharpeLbl);
    addRow(3, "Std Dev (daily)",   m_stdDevLbl);
    addRow(4, "VaR (95%, 1d)",     m_varLbl);
    addRow(5, "CVaR (95%, 1d)",    m_cvarLbl);
    addRow(6, "Kurtosis",          m_kurtosisLbl);
    addRow(7, "Skewness",          m_skewnessLbl);
    addRow(8, "Beta (vs SPY)",     m_betaLbl);
    addRow(9, "Alpha (vs SPY)",    m_alphaLbl);

    // ── Correlation matrix group (left panel) ─────────────────────────────────
    m_corrContainer = new QWidget(this);   // grid layout built dynamically

    auto* corrBox    = new QGroupBox("Correlation Matrix  (incl. SPY)", this);
    corrBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* corrBoxLay = new QVBoxLayout(corrBox);
    corrBoxLay->addWidget(m_corrContainer);
    corrBoxLay->addStretch(1);

    // ── Results area: corr matrix on left, scalar metrics on right ─────────────
    m_resultsWidget = new QWidget(this);
    auto* resLay = new QHBoxLayout(m_resultsWidget);
    resLay->setContentsMargins(0, 0, 0, 0);
    resLay->setSpacing(12);
    resLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    resLay->addWidget(corrBox,    0, Qt::AlignTop);
    resLay->addWidget(metricsBox, 0, Qt::AlignTop);
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

    // Lookback is a pure filter on already-fetched data — instant update is fine.
    connect(m_lookbackCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_pending == 0 && !m_prices.isEmpty()) computeAndDisplay();
    });
    // Risk-free affects Sharpe and Alpha; user applies it explicitly via Fetch.

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

    // ── Intersect trading dates across all symbols within the window ──────────
    QSet<QDate> commonSet;
    bool firstSym = true;
    for (const QString& sym : allSymbols) {
        const auto& pm = m_prices[sym];
        QSet<QDate> symDates;
        for (auto it = pm.lowerBound(startDate); it != pm.end() && it.key() <= endDate; ++it)
            symDates.insert(it.key());
        if (firstSym) { commonSet = symDates; firstSym = false; }
        else          { commonSet &= symDates; }
    }

    QList<QDate> sortedDates = commonSet.values();
    std::sort(sortedDates.begin(), sortedDates.end());
    const int nDays = sortedDates.size();

    if (nDays < 20) {
        emit statusMessage(
            QString("Only %1 common trading days in window — need at least 20.").arg(nDays));
        return;
    }

    // ── Per-symbol aligned daily returns ──────────────────────────────────────
    QMap<QString, QVector<double>> symReturns;
    for (const QString& sym : allSymbols) {
        const auto& pm = m_prices[sym];
        QVector<double> ret;
        ret.reserve(nDays);
        for (int i = 0; i < nDays; ++i) {
            const double p1 = pm.value(sortedDates[i]);
            double p0;
            if (i == 0) {
                auto it = pm.lowerBound(sortedDates[0]);
                p0 = (it != pm.begin()) ? (--it).value() : p1;
            } else {
                p0 = pm.value(sortedDates[i - 1]);
            }
            ret.append(p0 > 0.0 ? (p1 / p0 - 1.0) : 0.0);
        }
        symReturns[sym] = std::move(ret);
    }

    // ── Portfolio daily returns: weighted sum of individual symbol returns ─────
    const QMap<QString, double> weights = symbolWeights();
    QVector<double> portReturns(nDays, 0.0);
    for (const QString& sym : portSymbols) {
        if (!symReturns.contains(sym) || !weights.contains(sym)) continue;
        const double     w   = weights[sym];
        const auto&      ret = symReturns[sym];
        for (int i = 0; i < nDays; ++i) portReturns[i] += w * ret[i];
    }

    // ── Scalar metrics ────────────────────────────────────────────────────────
    const double riskFreeAnnual = m_riskFreeSpinBox->value() / 100.0;
    const RiskResult result = computePortfolioMetrics(
        portReturns, symReturns.value("SPY"), riskFreeAnnual);

    // ── Correlation matrix (all symbols incl. SPY) ────────────────────────────
    const QVector<QVector<double>> corrMx = computeCorrMatrix(allSymbols, symReturns);

    rebuildDisplay(result, allSymbols, corrMx);

    emit statusMessage(
        QString("Risk computed — %1 trading days | %2 symbol(s) | lookback: %3")
            .arg(nDays).arg(portSymbols.size()).arg(lbStr));
}

RiskWidget::RiskResult RiskWidget::computePortfolioMetrics(
    const QVector<double>& portRet,
    const QVector<double>& spyRet,
    double riskFreeAnnual) const
{
    RiskResult r;
    const int n = portRet.size();
    if (n < 5) return r;

    // Annual return (geometric compounding)
    double totalRet = 1.0;
    for (double x : portRet) totalRet *= (1.0 + x);
    r.annualReturn = std::pow(totalRet, 252.0 / n) - 1.0;

    // Daily std dev → annualised volatility
    r.stdDev    = std::sqrt(sVariance(portRet));
    r.annualVol = r.stdDev * std::sqrt(252.0);

    // Sharpe ratio
    r.sharpe = r.annualVol > 1e-12
        ? (r.annualReturn - riskFreeAnnual) / r.annualVol
        : 0.0;

    // VaR (95% confidence, 1-day) = 5th-percentile of sorted returns
    QVector<double> sorted = portRet;
    std::sort(sorted.begin(), sorted.end());
    const int varIdx = std::max(0, static_cast<int>(n * 0.05));
    r.var95 = sorted[varIdx];

    // CVaR = mean of all returns at or below VaR
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

    // Beta and Alpha vs SPY
    const int ns = qMin(n, spyRet.size());
    if (ns >= 5) {
        const QVector<double> pSub(portRet.cbegin(), portRet.cbegin() + ns);
        const QVector<double> sSub(spyRet.cbegin(),  spyRet.cbegin()  + ns);

        const double spyVar = sVariance(sSub);
        r.beta = spyVar > 1e-12 ? sCovariance(pSub, sSub) / spyVar : 0.0;

        double spyTotal = 1.0;
        for (double x : sSub) spyTotal *= (1.0 + x);
        const double spyAnnual = std::pow(spyTotal, 252.0 / ns) - 1.0;

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
                                 const QVector<QVector<double>>& corrMx)
{
    auto fmtVal = [](double v, int dp, bool pct) -> QString {
        if (!std::isfinite(v)) return "N/A";
        const double disp = pct ? v * 100.0 : v;
        return (v > 0.0 ? "+" : "") + QString::number(disp, 'f', dp) + (pct ? "%" : "");
    };

    auto setLbl = [&](QLabel* lbl, double v, int dp, bool pct, bool colorize) {
        lbl->setText(fmtVal(v, dp, pct));
        if (colorize && std::isfinite(v) && v != 0.0) {
            lbl->setStyleSheet(v > 0.0
                ? "color: #4CAF50; font-size: 14px; font-weight: bold;"
                : "color: #F44336; font-size: 14px; font-weight: bold;");
        } else {
            lbl->setStyleSheet("font-size: 14px;");
        }
    };

    setLbl(m_annualReturnLbl, r.annualReturn, 2, true,  true);
    setLbl(m_annualVolLbl,    r.annualVol,    2, true,  false);
    setLbl(m_sharpeLbl,       r.sharpe,       3, false, true);
    setLbl(m_stdDevLbl,       r.stdDev,       4, true,  false);
    setLbl(m_varLbl,          r.var95,        2, true,  false);
    setLbl(m_cvarLbl,         r.cvar95,       2, true,  false);
    setLbl(m_kurtosisLbl,     r.kurtosis,     3, false, false);
    setLbl(m_skewnessLbl,     r.skewness,     3, false, false);
    setLbl(m_betaLbl,         r.beta,         3, false, false);
    setLbl(m_alphaLbl,        r.alpha,        2, true,  true);

    // ── Rebuild correlation matrix grid ───────────────────────────────────────
    if (QLayout* old = m_corrContainer->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0))) {
            delete item->widget();
            delete item;
        }
        delete old;
    }

    const int n = symbols.size();
    auto* grid = new QGridLayout(m_corrContainer);
    grid->setSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    // Column headers (row 0, columns 1…n)
    for (int j = 0; j < n; ++j) {
        auto* hdr = new QLabel(symbols[j], m_corrContainer);
        hdr->setAlignment(Qt::AlignCenter);
        hdr->setStyleSheet("font-weight: bold; padding: 2px 6px;");
        grid->addWidget(hdr, 0, j + 1);
    }

    // Data rows: row header + coloured cells
    for (int i = 0; i < n; ++i) {
        auto* rhdr = new QLabel(symbols[i], m_corrContainer);
        rhdr->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rhdr->setStyleSheet("font-weight: bold; padding: 2px 6px;");
        grid->addWidget(rhdr, i + 1, 0);

        for (int j = 0; j < n; ++j) {
            const double v  = (i < corrMx.size() && j < corrMx[i].size())
                ? corrMx[i][j] : 0.0;
            const QColor bg = corrColor(v);
            // Perceived luminance — choose white or black text accordingly.
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

    m_resultsWidget->setVisible(true);
}

// ── weights ───────────────────────────────────────────────────────────────────

QMap<QString, double> RiskWidget::symbolWeights() const
{
    QMap<QString, double> weights;
    if (m_positions.isEmpty()) return weights;

    double totalW = 0.0;
    for (const auto& pos : m_positions) totalW += pos.originalWeight;

    if (totalW > 0.5) {
        for (const auto& pos : m_positions)
            weights[pos.symbol] += pos.originalWeight / 100.0;   // % → decimal
    } else {
        const double eq = 1.0 / m_positions.size();
        for (const auto& pos : m_positions)
            weights[pos.symbol] += eq;
    }

    // Normalise so weights always sum to 1.0
    double sum = 0.0;
    for (double w : weights) sum += w;
    if (sum > 1e-10) for (auto& w : weights) w /= sum;

    return weights;
}
