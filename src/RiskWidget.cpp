#include "RiskWidget.h"

#include <QLabel>
#include <QVBoxLayout>

RiskWidget::RiskWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* l   = new QVBoxLayout(this);
    auto* lbl = new QLabel("Risk — coming soon", this);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color: gray; font-size: 18px;");
    l->addWidget(lbl);
}
