#include "SymbolSearchEdit.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTimer>

SymbolSearchEdit::SymbolSearchEdit(QWidget* parent)
    : QWidget(parent)
    , m_typeCombo(new QComboBox(this))
    , m_symbolCombo(new QComboBox(this))
    , m_searchTimer(new QTimer(this))
    , m_searchClient(new SymbolSearchClient(this))
{
    m_typeCombo->addItems({"All", "Stock", "ETF", "Index", "Commodity", "Crypto", "Mutual Fund"});
    m_typeCombo->setFixedWidth(108);

    m_symbolCombo->setEditable(true);
    m_symbolCombo->setInsertPolicy(QComboBox::NoInsert);
    m_symbolCombo->setMinimumWidth(220);
    m_symbolCombo->setCompleter(nullptr); // disable built-in prefix completer
    m_symbolCombo->lineEdit()->setPlaceholderText("Symbol or name...");
    m_symbolCombo->lineEdit()->setClearButtonEnabled(true);

    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(m_typeCombo);
    lay->addWidget(m_symbolCombo, 1);

    // User types → debounce → search
    connect(m_symbolCombo->lineEdit(), &QLineEdit::textEdited,
            this, &SymbolSearchEdit::onTextEdited);

    // User selects an item from the dropdown (mouse click or Enter while open)
    connect(m_symbolCombo, qOverload<int>(&QComboBox::activated),
            this, &SymbolSearchEdit::onItemActivated);

    // User presses Enter while the popup is NOT open → raw symbol entry
    connect(m_symbolCombo->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
        if (m_symbolCombo->view()->isVisible()) return; // handled by activated()
        const QString sym = symbol();
        if (!sym.isEmpty())
            emit symbolConfirmed(sym, "");
    });

    connect(m_searchTimer, &QTimer::timeout,
            this, &SymbolSearchEdit::onSearchTimerTimeout);

    connect(m_searchClient, &SymbolSearchClient::resultsReady,
            this, &SymbolSearchEdit::onSearchResults);

    // Re-search when type filter changes (if there's already text)
    connect(m_typeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (!m_symbolCombo->lineEdit()->text().trimmed().isEmpty())
            m_searchTimer->start();
    });
}

QString SymbolSearchEdit::symbol() const
{
    return m_symbolCombo->lineEdit()->text().trimmed().toUpper();
}

QString SymbolSearchEdit::symbolName() const
{
    return m_lastName;
}

void SymbolSearchEdit::setSymbol(const QString& sym)
{
    m_ignoreEdit = true;
    m_symbolCombo->lineEdit()->setText(sym);
    m_ignoreEdit = false;
    m_lastName.clear();
}

void SymbolSearchEdit::clear()
{
    m_searchTimer->stop();
    m_searchClient->cancel();
    m_ignoreEdit = true;
    m_symbolCombo->hidePopup();
    m_symbolCombo->clear();
    m_symbolCombo->lineEdit()->clear();
    m_ignoreEdit = false;
    m_lastName.clear();
}

void SymbolSearchEdit::onTextEdited(const QString& text)
{
    if (m_ignoreEdit) return;
    m_lastName.clear();
    if (text.trimmed().length() >= 1) {
        m_searchTimer->start();
    } else {
        m_searchTimer->stop();
        m_searchClient->cancel();
        m_symbolCombo->hidePopup();
    }
}

void SymbolSearchEdit::onSearchTimerTimeout()
{
    m_searchClient->search(m_symbolCombo->lineEdit()->text(),
                           m_typeCombo->currentText());
}

void SymbolSearchEdit::onSearchResults(const QList<SymbolResult>& results)
{
    const QString typed = m_symbolCombo->lineEdit()->text();

    m_ignoreEdit = true;
    m_symbolCombo->blockSignals(true);
    m_symbolCombo->clear();
    for (const SymbolResult& r : results) {
        const QString display =
            QString("%1  —  %2  [%3]").arg(r.symbol, r.name, r.type);
        m_symbolCombo->addItem(display, QVariant::fromValue(r));
    }
    m_symbolCombo->lineEdit()->setText(typed); // restore what the user typed
    m_symbolCombo->blockSignals(false);
    m_ignoreEdit = false;

    if (!results.isEmpty())
        m_symbolCombo->showPopup();
    else
        m_symbolCombo->hidePopup();
}

void SymbolSearchEdit::onItemActivated(int index)
{
    if (index < 0 || index >= m_symbolCombo->count()) return;
    const QVariant v = m_symbolCombo->itemData(index);
    if (!v.isValid()) return;

    const SymbolResult r = v.value<SymbolResult>();
    m_lastName = r.name;

    m_ignoreEdit = true;
    m_symbolCombo->lineEdit()->setText(r.symbol);
    m_ignoreEdit = false;

    m_symbolCombo->hidePopup();
    emit symbolConfirmed(r.symbol, r.name);
}
