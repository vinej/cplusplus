#include "AiAssistantWidget.h"

#include <QLabel>
#include <QVBoxLayout>

AiAssistantWidget::AiAssistantWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* l   = new QVBoxLayout(this);
    auto* lbl = new QLabel("AI Assistant — coming soon", this);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color: gray; font-size: 18px;");
    l->addWidget(lbl);
}
