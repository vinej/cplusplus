#include "PortfolioWidget.h"

#include <QLabel>
#include <QVBoxLayout>

PortfolioWidget::PortfolioWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* l   = new QVBoxLayout(this);
    auto* lbl = new QLabel("Portfolio — coming soon", this);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color: gray; font-size: 18px;");
    l->addWidget(lbl);
}
