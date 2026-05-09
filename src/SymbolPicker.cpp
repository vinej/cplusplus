#include "SymbolPicker.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

SymbolPicker::SymbolPicker(QWidget* parent)
    : QWidget(parent)
    , m_symbolEdit(new QLineEdit("AAPL", this))
    , m_rangeCombo(new QComboBox(this))
    , m_intervalCombo(new QComboBox(this))
    , m_fetchButton(new QPushButton("Fetch", this))
{
    m_rangeCombo->addItems({"1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "max"});
    m_rangeCombo->setCurrentText("1y");

    m_intervalCombo->addItems({"1m", "5m", "15m", "30m", "60m", "90m", "1d", "1wk", "1mo"});
    m_intervalCombo->setCurrentText("1d");

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel("Symbol:", this));
    row->addWidget(m_symbolEdit);
    row->addWidget(new QLabel("Range:", this));
    row->addWidget(m_rangeCombo);
    row->addWidget(new QLabel("Interval:", this));
    row->addWidget(m_intervalCombo);
    row->addWidget(m_fetchButton);

    connect(m_symbolEdit, &QLineEdit::returnPressed,
            this, &SymbolPicker::fetchRequested);
    connect(m_fetchButton, &QPushButton::clicked,
            this, &SymbolPicker::fetchRequested);
    connect(m_rangeCombo, &QComboBox::currentTextChanged,
            this, &SymbolPicker::rangeChanged);
    connect(m_intervalCombo, &QComboBox::currentTextChanged,
            this, &SymbolPicker::intervalChanged);
}

QString SymbolPicker::symbol()   const { return m_symbolEdit->text().trimmed().toUpper(); }
QString SymbolPicker::range()    const { return m_rangeCombo->currentText(); }
QString SymbolPicker::interval() const { return m_intervalCombo->currentText(); }

QDate SymbolPicker::startDate() const
{
    const QDate today = QDate::currentDate();
    const QString r   = range();
    if (r == "1d")  return today.addDays(-1);
    if (r == "5d")  return today.addDays(-5);
    if (r == "1mo") return today.addMonths(-1);
    if (r == "3mo") return today.addMonths(-3);
    if (r == "6mo") return today.addMonths(-6);
    if (r == "1y")  return today.addYears(-1);
    if (r == "2y")  return today.addYears(-2);
    if (r == "5y")  return today.addYears(-5);
    if (r == "10y") return today.addYears(-10);
    return QDate(1970, 1, 1); // "max" or unknown
}

void SymbolPicker::setFetchEnabled(bool enabled)
{
    m_fetchButton->setEnabled(enabled);
}
