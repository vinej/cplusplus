#include "RebalanceDialog.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

enum Col { ColSym=0, ColPrice, ColCurWt, ColNewWt, ColNewQty, ColCount };

static QTableWidgetItem* roItem(const QString& text)
{
    auto* it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsEnabled);
    return it;
}

RebalanceDialog::RebalanceDialog(const QVector<PortfolioPosition>& positions,
                                 const QMap<QString,double>& currentPrices,
                                 QWidget* parent)
    : QDialog(parent)
    , m_positions(positions)
    , m_currentPrices(currentPrices)
{
    setWindowTitle("Rebalance Portfolio");
    setMinimumWidth(560);

    // ── Current total market value and weights ────────────────────────────────
    double totalVal = 0.0;
    for (const auto& pos : m_positions)
        totalVal += pos.quantity * m_currentPrices.value(pos.symbol, 0.0);

    QMap<QString,double> curWeights;
    if (totalVal > 0) {
        for (const auto& pos : m_positions)
            curWeights[pos.symbol] =
                pos.quantity * m_currentPrices.value(pos.symbol, 0.0) / totalVal * 100.0;
    }

    // ── Top bar ───────────────────────────────────────────────────────────────
    m_totalSpin = new QDoubleSpinBox(this);
    m_totalSpin->setRange(0.0, 1e9);
    m_totalSpin->setDecimals(2);
    m_totalSpin->setPrefix("$");
    m_totalSpin->setSingleStep(1000.0);
    m_totalSpin->setValue(totalVal);

    auto* normalizeBtn = new QPushButton("Normalize to 100%", this);
    auto* resetBtn     = new QPushButton("Reset to Original Weights", this);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel("Portfolio Value:", this));
    topRow->addWidget(m_totalSpin);
    topRow->addStretch();
    topRow->addWidget(normalizeBtn);
    topRow->addWidget(resetBtn);

    // ── Grid ─────────────────────────────────────────────────────────────────
    m_grid = new QTableWidget(m_positions.size(), ColCount, this);
    m_grid->setHorizontalHeaderLabels(
        {"Symbol", "Price", "Cur Wt%", "New Wt%", "New Qty"});
    m_grid->horizontalHeader()->setSectionResizeMode(ColSym,   QHeaderView::ResizeToContents);
    m_grid->horizontalHeader()->setSectionResizeMode(ColPrice, QHeaderView::ResizeToContents);
    m_grid->horizontalHeader()->setSectionResizeMode(ColCurWt, QHeaderView::ResizeToContents);
    m_grid->horizontalHeader()->setSectionResizeMode(ColNewWt, QHeaderView::Stretch);
    m_grid->horizontalHeader()->setSectionResizeMode(ColNewQty,QHeaderView::ResizeToContents);
    m_grid->verticalHeader()->hide();
    m_grid->setSelectionMode(QAbstractItemView::NoSelection);
    m_grid->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int i = 0; i < m_positions.size(); ++i) {
        const auto& pos   = m_positions[i];
        const double price = m_currentPrices.value(pos.symbol, 0.0);

        m_grid->setItem(i, ColSym,
            roItem(pos.symbol));
        m_grid->setItem(i, ColPrice,
            roItem(price > 0 ? QString("$%1").arg(price, 0, 'f', 2) : "—"));
        m_grid->setItem(i, ColCurWt,
            roItem(QString("%1%").arg(curWeights.value(pos.symbol, 0.0), 0, 'f', 2)));

        auto* spin = new QDoubleSpinBox(m_grid);
        spin->setRange(0.0, 100.0);
        spin->setDecimals(2);
        spin->setSingleStep(1.0);
        spin->setSuffix("%");
        const double initWt = pos.originalWeight > 0
            ? pos.originalWeight
            : curWeights.value(pos.symbol, 0.0);
        spin->setValue(initWt);
        m_grid->setCellWidget(i, ColNewWt, spin);
        m_weightSpins.append(spin);

        m_grid->setItem(i, ColNewQty, roItem("—"));

        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this, i](double) { updateRow(i); updateSum(); });
    }

    // ── Sum label + buttons ───────────────────────────────────────────────────
    m_sumLabel = new QLabel(this);
    m_sumLabel->setAlignment(Qt::AlignRight);

    m_applyBtn      = new QPushButton("Apply", this);
    auto* cancelBtn = new QPushButton("Cancel", this);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(m_applyBtn);
    btnRow->addWidget(cancelBtn);

    // ── Root layout ───────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->addLayout(topRow);
    root->addWidget(m_grid, 1);
    root->addWidget(m_sumLabel);
    root->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_totalSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double) {
                for (int i = 0; i < m_positions.size(); ++i) updateRow(i);
            });

    connect(normalizeBtn, &QPushButton::clicked, this, [this]() {
        double sum = 0.0;
        for (auto* s : m_weightSpins) sum += s->value();
        if (sum <= 0.0) return;
        for (auto* s : m_weightSpins) {
            s->blockSignals(true);
            s->setValue(s->value() / sum * 100.0);
            s->blockSignals(false);
        }
        for (int i = 0; i < m_positions.size(); ++i) updateRow(i);
        updateSum();
    });

    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_weightSpins.size(); ++i) {
            m_weightSpins[i]->blockSignals(true);
            m_weightSpins[i]->setValue(m_positions[i].originalWeight);
            m_weightSpins[i]->blockSignals(false);
        }
        for (int i = 0; i < m_positions.size(); ++i) updateRow(i);
        updateSum();
    });

    connect(m_applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);

    // Initial render
    for (int i = 0; i < m_positions.size(); ++i) updateRow(i);
    updateSum();
}

void RebalanceDialog::updateRow(int i)
{
    const double price    = m_currentPrices.value(m_positions[i].symbol, 0.0);
    const double totalVal = m_totalSpin->value();
    const double wt       = m_weightSpins[i]->value();

    auto* item = m_grid->item(i, ColNewQty);
    if (price > 0.0 && totalVal > 0.0 && wt > 0.0)
        item->setText(QString::number((wt / 100.0) * totalVal / price, 'f', 4));
    else
        item->setText("—");
}

void RebalanceDialog::updateSum()
{
    double sum = 0.0;
    for (auto* s : m_weightSpins) sum += s->value();

    const bool over  = sum > 100.01;
    const bool exact = !over && std::abs(sum - 100.0) < 0.01;

    QString label = QString("Weights sum: %1%").arg(sum, 0, 'f', 2);
    if (over)
        label += "  — exceeds 100%, reduce weights or use Normalize";
    else if (exact)
        label += "  ✓";
    else
        label += QString("  (%1% unallocated)").arg(100.0 - sum, 0, 'f', 2);

    m_sumLabel->setText(label);
    m_sumLabel->setStyleSheet(
        over  ? "color: #f44336;" :
        exact ? "color: #4caf50;" :
                "color: #ff9800;");          // orange = partial allocation

    // Apply is valid when there's something to allocate and sum doesn't exceed 100%.
    m_applyBtn->setEnabled(!over && sum > 0.0);
}

QMap<QString,double> RebalanceDialog::newQuantities() const
{
    const double totalVal = m_totalSpin->value();
    QMap<QString,double> result;
    for (int i = 0; i < m_positions.size(); ++i) {
        const double price = m_currentPrices.value(m_positions[i].symbol, 0.0);
        if (price <= 0.0) continue;
        result[m_positions[i].symbol] =
            (m_weightSpins[i]->value() / 100.0) * totalVal / price;
    }
    return result;
}
