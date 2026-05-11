#include "PortfolioWidget.h"

#include "RebalanceDialog.h"
#include "SymbolSearchEdit.h"
#include "YahooFinanceClient.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

// ── grid column indices ───────────────────────────────────────────────────────

enum Col {
    ColSymbol = 0, ColName, ColQty, ColCost, ColDate,
    ColMktVal, ColOrigWt, ColCurWt, ColPL,
    ColCount
};

// ── helpers ───────────────────────────────────────────────────────────────────

static QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

// ── PortfolioWidget ───────────────────────────────────────────────────────────

PortfolioWidget::PortfolioWidget(QWidget* parent)
    : QWidget(parent)
    , m_client(new YahooFinanceClient(this))
    , m_portfolioCombo(new QComboBox(this))
    , m_newBtn(new QPushButton("New", this))
    , m_saveBtn(new QPushButton("Save", this))
    , m_saveAsBtn(new QPushButton("Save As...", this))
    , m_deleteBtn(new QPushButton("Delete", this))
    , m_symbolSearch(new SymbolSearchEdit(this))
    , m_tickerNameLabel(new QLabel(this))
    , m_qtySpin(new QDoubleSpinBox(this))
    , m_costSpin(new QDoubleSpinBox(this))
    , m_dateEdit(new QDateEdit(QDate::currentDate(), this))
    , m_addBtn(new QPushButton("Add to Portfolio", this))
    , m_totalValueLabel(new QLabel("Total Market Value: —", this))
    , m_totalPlLabel(new QLabel("Total P/L: —", this))
    , m_refreshBtn(new QPushButton("Refresh Prices", this))
    , m_editBtn(new QPushButton("Edit Selected", this))
    , m_removeBtn(new QPushButton("Remove Selected", this))
    , m_rebalanceBtn(new QPushButton("Rebalance...", this))
    , m_saveWeightBtn(new QPushButton("Save Weight as Original", this))
    , m_grid(new QTableWidget(0, ColCount, this))
{
    // Open database
    if (!PortfolioDb::instance().open())
        emit statusMessage("DB error: " + PortfolioDb::instance().lastError());

    // ── Widget setup ──────────────────────────────────────────────────────────
    m_portfolioCombo->setMinimumWidth(200);

    m_tickerNameLabel->setStyleSheet("color: gray; font-style: italic;");

    m_qtySpin->setRange(0.0001, 99'999'999.0);
    m_qtySpin->setDecimals(4);
    m_qtySpin->setValue(1.0);

    m_costSpin->setRange(0.01, 9'999'999.99);
    m_costSpin->setDecimals(2);
    m_costSpin->setPrefix("$");

    m_dateEdit->setCalendarPopup(true);
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);

    // Grid
    m_grid->setHorizontalHeaderLabels(
        {"Symbol", "Name", "Qty", "Cost/Share", "Date Acquired",
         "Mkt Value", "Orig Wt%", "Cur Wt%", "Total P/L"});
    m_grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_grid->setColumnWidth(ColSymbol,  65);
    m_grid->setColumnWidth(ColName,   300);
    m_grid->setColumnWidth(ColQty,     85);
    m_grid->setColumnWidth(ColCost,    80);
    m_grid->setColumnWidth(ColDate,    95);
    m_grid->setColumnWidth(ColMktVal,  90);
    m_grid->setColumnWidth(ColOrigWt,  70);
    m_grid->setColumnWidth(ColCurWt,   70);
    m_grid->setColumnWidth(ColPL, 100);
    m_grid->verticalHeader()->hide();
    m_grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_grid->setSortingEnabled(false);

    // ── Layouts ───────────────────────────────────────────────────────────────
    auto* s1 = new QHBoxLayout();
    s1->addWidget(new QLabel("Portfolio:", this));
    s1->addWidget(m_portfolioCombo);
    s1->addWidget(m_newBtn);
    s1->addWidget(m_saveBtn);
    s1->addWidget(m_saveAsBtn);
    s1->addWidget(m_deleteBtn);
    s1->addStretch();

    auto* s2r1 = new QHBoxLayout();
    s2r1->addWidget(new QLabel("Ticker:", this));
    s2r1->addWidget(m_symbolSearch);
    s2r1->addWidget(m_tickerNameLabel, 1);
    s2r1->addStretch();

    auto* s2r2 = new QHBoxLayout();
    s2r2->addWidget(new QLabel("Qty:", this));
    s2r2->addWidget(m_qtySpin);
    s2r2->addWidget(new QLabel("Cost:", this));
    s2r2->addWidget(m_costSpin);
    s2r2->addWidget(new QLabel("Date:", this));
    s2r2->addWidget(m_dateEdit);
    s2r2->addWidget(m_addBtn);
    s2r2->addStretch();

    auto* s3 = new QHBoxLayout();
    s3->addWidget(m_totalValueLabel);
    s3->addSpacing(24);
    s3->addWidget(m_totalPlLabel);
    s3->addStretch();

    auto* s4 = new QHBoxLayout();
    s4->addWidget(m_refreshBtn);
    s4->addWidget(m_editBtn);
    s4->addWidget(m_removeBtn);
    s4->addWidget(m_rebalanceBtn);
    s4->addWidget(m_saveWeightBtn);
    s4->addStretch();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);
    root->addLayout(s1);
    root->addLayout(s2r1);
    root->addLayout(s2r2);
    root->addLayout(s3);
    root->addLayout(s4);
    root->addWidget(m_grid, 1);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_portfolioCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PortfolioWidget::onPortfolioSelected);
    connect(m_newBtn,          &QPushButton::clicked, this, &PortfolioWidget::onNew);
    connect(m_saveBtn,         &QPushButton::clicked, this, &PortfolioWidget::onSave);
    connect(m_saveAsBtn,       &QPushButton::clicked, this, &PortfolioWidget::onSaveAs);
    connect(m_deleteBtn,       &QPushButton::clicked, this, &PortfolioWidget::onDelete);
    connect(m_symbolSearch, &SymbolSearchEdit::symbolConfirmed,
            this, &PortfolioWidget::onTickerConfirmed);
    connect(m_addBtn,          &QPushButton::clicked, this, &PortfolioWidget::onAddToPortfolio);
    connect(m_refreshBtn,      &QPushButton::clicked, this, &PortfolioWidget::onRefreshPrices);
    connect(m_editBtn,         &QPushButton::clicked, this, &PortfolioWidget::onEditSelected);
    connect(m_removeBtn,       &QPushButton::clicked, this, &PortfolioWidget::onRemoveSelected);
    connect(m_rebalanceBtn,    &QPushButton::clicked, this, &PortfolioWidget::onRebalance);
    connect(m_saveWeightBtn,   &QPushButton::clicked, this, &PortfolioWidget::onSaveWeightAsOriginal);

    connect(m_client, &YahooFinanceClient::finished, this, &PortfolioWidget::onFetchFinished);
    connect(m_client, &YahooFinanceClient::failed,   this, &PortfolioWidget::onFetchFailed);

    loadPortfolios();
}

// ── portfolio management ──────────────────────────────────────────────────────

void PortfolioWidget::loadPortfolios()
{
    m_portfolioCombo->blockSignals(true);
    m_portfolioCombo->clear();
    m_portfolioCombo->addItem("— Select portfolio —", -1);
    for (const auto& p : PortfolioDb::instance().portfolios())
        m_portfolioCombo->addItem(p.second, p.first);
    m_portfolioCombo->blockSignals(false);
}

void PortfolioWidget::onPortfolioSelected(int index)
{
    const int id = m_portfolioCombo->itemData(index).toInt();
    m_currentPortfolioId = id;
    m_positions.clear();
    m_currentPrices.clear();
    m_deleteBtn->setEnabled(id > 0);
    m_dirty = false;
    m_saveBtn->setEnabled(false);

    if (id > 0) {
        m_positions = PortfolioDb::instance().positions(id);
        clearInputs();
        refreshGrid();
        onRefreshPrices();
    } else {
        clearInputs();
        refreshGrid();
    }
}

void PortfolioWidget::onNew()
{
    m_currentPortfolioId = -1;
    m_positions.clear();
    m_currentPrices.clear();
    m_portfolioCombo->blockSignals(true);
    m_portfolioCombo->setCurrentIndex(0);
    m_portfolioCombo->blockSignals(false);
    m_deleteBtn->setEnabled(false);
    markDirty();
    clearInputs();
    refreshGrid();
}

void PortfolioWidget::onSave()
{
    auto& db = PortfolioDb::instance();

    // ── Existing portfolio ────────────────────────────────────────────────────
    if (m_currentPortfolioId > 0) {
        if (!db.savePositions(m_currentPortfolioId, m_positions)) {
            QMessageBox::warning(this, "Save Error", db.lastError());
            return;
        }
        m_positions = db.positions(m_currentPortfolioId);
        m_dirty = false;
        m_saveBtn->setEnabled(false);
        refreshGrid();
        emit statusMessage("Portfolio saved.");
        return;
    }

    // ── New portfolio ─────────────────────────────────────────────────────────
    bool ok;
    QString name = QInputDialog::getText(
        this, "Save Portfolio", "Portfolio name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    // Check for existing portfolio with same name
    for (const auto& p : db.portfolios()) {
        if (p.second.compare(name, Qt::CaseInsensitive) == 0) {
            if (QMessageBox::question(this, "Overwrite?",
                    QString("Portfolio \"%1\" already exists. Overwrite?").arg(name))
                    != QMessageBox::Yes)
                return;
            m_currentPortfolioId = p.first;
            break;
        }
    }

    if (m_currentPortfolioId == -1) {
        m_currentPortfolioId = db.createPortfolio(name);
        if (m_currentPortfolioId == -1) {
            QMessageBox::warning(this, "Save Error", db.lastError());
            return;
        }
    }

    if (!db.savePositions(m_currentPortfolioId, m_positions)) {
        QMessageBox::warning(this, "Save Error", db.lastError());
        return;
    }

    m_positions = db.positions(m_currentPortfolioId);
    loadPortfolios();

    // Re-select the saved portfolio in the combo
    for (int i = 0; i < m_portfolioCombo->count(); ++i) {
        if (m_portfolioCombo->itemData(i).toInt() == m_currentPortfolioId) {
            m_portfolioCombo->blockSignals(true);
            m_portfolioCombo->setCurrentIndex(i);
            m_portfolioCombo->blockSignals(false);
            break;
        }
    }

    m_dirty = false;
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setEnabled(true);
    refreshGrid();
    emit statusMessage("Portfolio saved.");
}

void PortfolioWidget::onSaveAs()
{
    auto& db = PortfolioDb::instance();

    bool ok;
    const QString defaultName = m_currentPortfolioId > 0
        ? m_portfolioCombo->currentText() : QString();
    QString name = QInputDialog::getText(
        this, "Save As", "Portfolio name:", QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    // Look up whether a portfolio with this name already exists.
    int existingId = -1;
    for (const auto& p : db.portfolios()) {
        if (p.second.compare(name, Qt::CaseInsensitive) == 0) {
            existingId = p.first;
            break;
        }
    }

    int targetId = existingId;

    if (existingId != -1 && existingId != m_currentPortfolioId) {
        // A DIFFERENT portfolio has this name — ask to overwrite.
        if (QMessageBox::question(this, "Overwrite?",
                QString("Portfolio \"%1\" already exists. Overwrite?").arg(name))
                != QMessageBox::Yes)
            return;
    } else if (existingId == -1) {
        // Brand-new name — create the portfolio.
        targetId = db.createPortfolio(name);
        if (targetId == -1) {
            QMessageBox::warning(this, "Save Error", db.lastError());
            return;
        }
    }
    // else existingId == m_currentPortfolioId: same name → behaves like Save.

    if (!db.savePositions(targetId, m_positions)) {
        QMessageBox::warning(this, "Save Error", db.lastError());
        return;
    }

    m_currentPortfolioId = targetId;
    m_positions = db.positions(m_currentPortfolioId);
    loadPortfolios();

    for (int i = 0; i < m_portfolioCombo->count(); ++i) {
        if (m_portfolioCombo->itemData(i).toInt() == m_currentPortfolioId) {
            m_portfolioCombo->blockSignals(true);
            m_portfolioCombo->setCurrentIndex(i);
            m_portfolioCombo->blockSignals(false);
            break;
        }
    }

    m_dirty = false;
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setEnabled(true);
    refreshGrid();
    emit statusMessage(QString("Portfolio saved as \"%1\".").arg(name));
}

void PortfolioWidget::onDelete()
{
    if (m_currentPortfolioId == -1) return;

    const QString name = m_portfolioCombo->currentText();
    if (QMessageBox::question(this, "Delete Portfolio",
            QString("Delete portfolio \"%1\"?\nThis cannot be undone.").arg(name))
            != QMessageBox::Yes)
        return;

    if (!PortfolioDb::instance().deletePortfolio(m_currentPortfolioId)) {
        QMessageBox::warning(this, "Delete Error", PortfolioDb::instance().lastError());
        return;
    }

    m_currentPortfolioId = -1;
    m_positions.clear();
    m_currentPrices.clear();
    loadPortfolios();
    clearInputs();
    refreshGrid();
    emit statusMessage("Portfolio deleted.");
}

// ── position input ────────────────────────────────────────────────────────────

void PortfolioWidget::onTickerConfirmed(const QString& symbol, const QString& name)
{
    if (symbol.isEmpty()) return;
    // Name from dropdown is already known; set it immediately
    if (!name.isEmpty())
        m_tickerNameLabel->setText(name);
    emit statusMessage(QString("Fetching price for %1...").arg(symbol));
    m_client->fetch(symbol, "1d",
                    QDate::currentDate().addDays(-7), QDate::currentDate(), "ticker");
}

void PortfolioWidget::onAddToPortfolio()
{
    const QString symbol = m_symbolSearch->symbol();
    if (symbol.isEmpty()) return;

    PortfolioPosition pos;
    pos.symbol = symbol;
    const QString fetchedName = m_tickerNameLabel->text().trimmed();
    pos.name   = fetchedName.isEmpty() ? symbol : fetchedName;
    pos.quantity     = m_qtySpin->value();
    pos.cost         = m_costSpin->value();
    pos.dateAcquired = m_dateEdit->date();

    if (m_editingExisting && m_editingPositionIdx >= 0
            && m_editingPositionIdx < m_positions.size()) {
        pos.id             = m_positions[m_editingPositionIdx].id;
        pos.originalWeight = m_positions[m_editingPositionIdx].originalWeight;
        m_positions[m_editingPositionIdx] = pos;
        m_editingExisting    = false;
        m_editingPositionIdx = -1;
        m_addBtn->setText("Add to Portfolio");
    } else {
        pos.originalWeight = 0.0;
        m_positions.append(pos);
    }

    markDirty();
    clearInputs();
    refreshGrid();
}

// ── grid actions ──────────────────────────────────────────────────────────────

void PortfolioWidget::onRefreshPrices()
{
    if (m_positions.isEmpty()) return;

    // For each symbol find the earliest purchase date so we can fetch the full
    // adjClose history needed to compute dividend-adjusted P/L.
    QMap<QString, QDate> earliest;
    for (const auto& pos : m_positions) {
        const QDate d = pos.dateAcquired.isValid() ? pos.dateAcquired : QDate::currentDate();
        if (!earliest.contains(pos.symbol) || d < earliest[pos.symbol])
            earliest[pos.symbol] = d;
    }

    const QStringList symbols = earliest.keys();
    m_pendingRefreshCount = symbols.size();
    m_refreshBtn->setEnabled(false);
    m_adjHistory.clear();

    const QDate today = QDate::currentDate();
    for (const QString& sym : symbols)
        m_client->fetch(sym, "1d", earliest[sym].addDays(-5), today, "refresh:" + sym);

    emit statusMessage(QString("Refreshing prices for %1 symbol(s)...").arg(symbols.size()));
}

void PortfolioWidget::onEditSelected()
{
    const int row = m_grid->currentRow();
    if (row < 0 || row >= m_positions.size()) return;

    const auto& pos = m_positions[row];
    m_symbolSearch->setSymbol(pos.symbol);
    m_tickerNameLabel->setText(pos.name != pos.symbol ? pos.name : "");
    m_qtySpin->setValue(pos.quantity);
    m_costSpin->setValue(pos.cost);
    m_dateEdit->setDate(pos.dateAcquired.isValid() ? pos.dateAcquired : QDate::currentDate());

    m_editingExisting    = true;
    m_editingPositionIdx = row;
    m_addBtn->setText("Update Position");
}

void PortfolioWidget::onRemoveSelected()
{
    const int row = m_grid->currentRow();
    if (row < 0 || row >= m_positions.size()) return;

    const QString sym = m_positions[row].symbol;
    if (QMessageBox::question(this, "Remove Position",
            QString("Remove %1 from portfolio?").arg(sym)) != QMessageBox::Yes)
        return;

    m_positions.removeAt(row);

    // Cancel any active edit if it pointed to this row or beyond
    if (m_editingExisting && m_editingPositionIdx >= row) {
        m_editingExisting    = false;
        m_editingPositionIdx = -1;
        m_addBtn->setText("Add to Portfolio");
        clearInputs();
    }

    markDirty();
    refreshGrid();
}

void PortfolioWidget::onRebalance()
{
    if (m_positions.isEmpty()) return;

    m_openRebalanceAfterRefresh = true;
    m_rebalanceBtn->setEnabled(false);
    onRefreshPrices();
}

void PortfolioWidget::openRebalanceDialog()
{
    m_openRebalanceAfterRefresh = false;
    m_rebalanceBtn->setEnabled(true);

    RebalanceDialog dlg(m_positions, m_currentPrices, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QMap<QString,double> newQtys = dlg.newQuantities();
    for (auto& pos : m_positions) {
        if (newQtys.contains(pos.symbol))
            pos.quantity = newQtys[pos.symbol];
    }
    markDirty();
    refreshGrid();
}

void PortfolioWidget::onSaveWeightAsOriginal()
{
    const double totalVal = totalMarketValue();
    if (totalVal <= 0) return;

    const QMap<QString,double> weights = currentWeights();
    for (auto& pos : m_positions)
        pos.originalWeight = weights.value(pos.symbol, 0.0);

    if (m_currentPortfolioId > 0)
        PortfolioDb::instance().updateOriginalWeights(m_currentPortfolioId, weights);

    refreshGrid();
}

// ── network callbacks ─────────────────────────────────────────────────────────

void PortfolioWidget::onFetchFinished(const QString& symbol,
                                       const QString& tag,
                                       const CandleSeries& candles,
                                       const QString& name)
{
    if (tag == "ticker") {
        if (!candles.isEmpty()) {
            const double price = candles.last().close;
            m_costSpin->setValue(price);
            // Only overwrite the name label if it wasn't already set from the dropdown
            if (m_tickerNameLabel->text().isEmpty())
                m_tickerNameLabel->setText(name.isEmpty() ? symbol : name);
            emit statusMessage(
                QString("%1: current price $%2").arg(symbol).arg(price, 0, 'f', 2));
        } else {
            emit statusMessage(QString("%1: no price data").arg(symbol));
        }
        return;
    }

    if (tag.startsWith("refresh:")) {
        if (!candles.isEmpty()) {
            // Store full adjClose history for dividend-adjusted P/L.
            auto& hist = m_adjHistory[symbol];
            for (const Candle& c : candles) {
                const double p = c.adjClose > 0.0 ? c.adjClose : c.close;
                if (p > 0.0) hist[c.timestamp.date()] = p;
            }
            // Current market price = most recent adjClose.
            if (!hist.isEmpty())
                m_currentPrices[symbol] = (hist.end() - 1).value();

            if (!name.isEmpty()) {
                for (auto& pos : m_positions)
                    if (pos.symbol == symbol && pos.name == symbol)
                        pos.name = name;
            }
        }
        if (--m_pendingRefreshCount <= 0) {
            m_refreshBtn->setEnabled(true);
            refreshGrid();
            emit statusMessage("Prices refreshed.");
            if (m_openRebalanceAfterRefresh)
                openRebalanceDialog();
        }
    }
}

void PortfolioWidget::onFetchFailed(const QString& symbol,
                                     const QString& tag,
                                     const QString& message)
{
    if (tag == "ticker") {
        emit statusMessage(QString("Failed to fetch %1: %2").arg(symbol, message));
    } else if (tag.startsWith("refresh:")) {
        if (--m_pendingRefreshCount <= 0) {
            m_refreshBtn->setEnabled(true);
            refreshGrid();
            if (m_openRebalanceAfterRefresh)
                openRebalanceDialog();
        }
    }
}

// ── grid refresh ──────────────────────────────────────────────────────────────

void PortfolioWidget::refreshGrid()
{
    m_grid->setRowCount(0);

    const double totalMktVal = totalMarketValue();

    for (const auto& pos : m_positions) {
        const double price    = m_currentPrices.value(pos.symbol, 0.0);
        const double mktVal   = pos.quantity * price;
        const double curWt    = (totalMktVal > 0) ? (mktVal / totalMktVal * 100.0) : 0.0;
        const double pl       = computePositionPL(pos);
        const bool   hasPrice = price > 0;

        const int row = m_grid->rowCount();
        m_grid->insertRow(row);

        m_grid->setItem(row, ColSymbol, readOnlyItem(pos.symbol));
        m_grid->setItem(row, ColName,   readOnlyItem(pos.name));
        m_grid->setItem(row, ColQty,    readOnlyItem(QString::number(pos.quantity, 'f', 4)));
        m_grid->setItem(row, ColCost,   readOnlyItem(QString("$%1").arg(pos.cost, 0, 'f', 2)));
        m_grid->setItem(row, ColDate,
            readOnlyItem(pos.dateAcquired.isValid()
                ? pos.dateAcquired.toString("yyyy-MM-dd") : "—"));
        m_grid->setItem(row, ColMktVal,
            readOnlyItem(hasPrice ? QString("$%1").arg(mktVal, 0, 'f', 2) : "—"));
        m_grid->setItem(row, ColOrigWt,
            readOnlyItem(QString("%1%").arg(pos.originalWeight, 0, 'f', 2)));
        m_grid->setItem(row, ColCurWt,
            readOnlyItem(hasPrice ? QString("%1%").arg(curWt, 0, 'f', 2) : "—"));

        auto* plItem = readOnlyItem(
            hasPrice ? QString("%1$%2").arg(pl >= 0 ? "+" : "-")
                                       .arg(std::abs(pl), 0, 'f', 2)
                     : "—");
        if (hasPrice)
            plItem->setForeground(pl >= 0 ? QColor("#4caf50") : QColor("#f44336"));
        m_grid->setItem(row, ColPL, plItem);
    }

    updateSummary();
}

void PortfolioWidget::updateSummary()
{
    const double totalMktVal = totalMarketValue();

    double totalCost = 0.0;
    double pl        = 0.0;
    for (const auto& pos : m_positions) {
        totalCost += pos.quantity * pos.cost;
        pl        += computePositionPL(pos);
    }
    const double plPct = (totalCost > 0) ? (pl / totalCost * 100.0) : 0.0;

    m_totalValueLabel->setText(
        totalMktVal > 0
            ? QString("Total Market Value: $%1").arg(totalMktVal, 0, 'f', 2)
            : "Total Market Value: —");

    if (totalCost > 0) {
        m_totalPlLabel->setText(
            QString("Total P/L: %1$%2 (%3%4%)")
                .arg(pl >= 0 ? "+" : "-")
                .arg(std::abs(pl), 0, 'f', 2)
                .arg(plPct >= 0 ? "+" : "")
                .arg(plPct, 0, 'f', 2));
        m_totalPlLabel->setStyleSheet(
            pl >= 0 ? "color: #4caf50; font-weight: bold;"
                    : "color: #f44336; font-weight: bold;");
    } else {
        m_totalPlLabel->setText("Total P/L: —");
        m_totalPlLabel->setStyleSheet("");
    }
}

// ── helpers ───────────────────────────────────────────────────────────────────

// Dividend-adjusted P/L: qty × cost × (adjClose_today / adjClose_at_purchase − 1).
// Falls back to simple (price − cost) × qty when historical data is unavailable.
double PortfolioWidget::computePositionPL(const PortfolioPosition& pos) const
{
    const double price = m_currentPrices.value(pos.symbol, 0.0);
    if (price <= 0.0) return 0.0;

    const auto& hist = m_adjHistory.value(pos.symbol);
    if (!hist.isEmpty() && pos.dateAcquired.isValid()) {
        // Find the adjClose at or just before the purchase date.
        auto it = hist.upperBound(pos.dateAcquired);
        if (it != hist.begin()) {
            const double adjAtPurchase = (--it).value();
            if (adjAtPurchase > 0.0)
                return pos.quantity * pos.cost * (price / adjAtPurchase - 1.0);
        }
    }
    return (price - pos.cost) * pos.quantity;
}

void PortfolioWidget::markDirty()
{
    m_dirty = true;
    m_saveBtn->setEnabled(true);
}

double PortfolioWidget::totalMarketValue() const
{
    double total = 0.0;
    for (const auto& pos : m_positions)
        total += pos.quantity * m_currentPrices.value(pos.symbol, 0.0);
    return total;
}

QMap<QString,double> PortfolioWidget::currentWeights() const
{
    QMap<QString,double> weights;
    const double total = totalMarketValue();
    if (total <= 0) return weights;
    for (const auto& pos : m_positions)
        weights[pos.symbol] =
            pos.quantity * m_currentPrices.value(pos.symbol, 0.0) / total * 100.0;
    return weights;
}

void PortfolioWidget::clearInputs()
{
    m_symbolSearch->clear();
    m_tickerNameLabel->clear();
    m_qtySpin->setValue(1.0);
    m_costSpin->setValue(0.01);
    m_dateEdit->setDate(QDate::currentDate());
    m_editingExisting    = false;
    m_editingPositionIdx = -1;
    m_addBtn->setText("Add to Portfolio");
}
