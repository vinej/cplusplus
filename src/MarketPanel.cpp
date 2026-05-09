#include "MarketPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QLocale>

MarketPanel::MarketPanel(QWidget* parent)
    : CollapsiblePanel("Current Market", parent)
{
    auto* grid = new QGridLayout(body());
    grid->setContentsMargins(8, 4, 8, 4);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(2);

    auto makeValue = [this]() {
        auto* l = new QLabel("—", body());
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };

    grid->addWidget(new QLabel("Date:",   body()), 0, 0); m_date  = makeValue(); grid->addWidget(m_date,  0, 1);
    grid->addWidget(new QLabel("Open:",   body()), 1, 0); m_open  = makeValue(); grid->addWidget(m_open,  1, 1);
    grid->addWidget(new QLabel("High:",   body()), 2, 0); m_high  = makeValue(); grid->addWidget(m_high,  2, 1);
    grid->addWidget(new QLabel("Low:",    body()), 3, 0); m_low   = makeValue(); grid->addWidget(m_low,   3, 1);
    grid->addWidget(new QLabel("Close:",  body()), 4, 0); m_close = makeValue(); grid->addWidget(m_close, 4, 1);
    grid->addWidget(new QLabel("Volume:", body()), 5, 0); m_vol   = makeValue(); grid->addWidget(m_vol,   5, 1);
    grid->setColumnStretch(1, 1);
}

void MarketPanel::update(const Candle& c)
{
    m_date ->setText(c.timestamp.toString("yyyy-MM-dd"));
    m_open ->setText(QString::number(c.open,  'f', 2));
    m_high ->setText(QString::number(c.high,  'f', 2));
    m_low  ->setText(QString::number(c.low,   'f', 2));
    m_close->setText(QString::number(c.close, 'f', 2));
    m_vol  ->setText(QLocale().toString(static_cast<qint64>(c.volume)));
}
