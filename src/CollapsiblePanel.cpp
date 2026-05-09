#include "CollapsiblePanel.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

CollapsiblePanel::CollapsiblePanel(const QString& title, QWidget* parent)
    : QFrame(parent), m_title(title)
{
    setFrameShape(QFrame::StyledPanel);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    m_header = new QPushButton("▼  " + title, this);
    m_header->setFlat(true);
    m_header->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  font-weight: bold;"
        "  padding: 4px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #555;"
        "}"
        "QPushButton:hover { background: rgba(255,255,255,15); }"
    );
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_header->setCursor(Qt::PointingHandCursor);

    m_body = new QWidget(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_header);
    root->addWidget(m_body);

    connect(m_header, &QPushButton::clicked, this, &CollapsiblePanel::onToggle);
}

QWidget* CollapsiblePanel::body() const { return m_body; }

void CollapsiblePanel::onToggle()
{
    m_expanded = !m_expanded;
    m_body->setVisible(m_expanded);
    m_header->setText((m_expanded ? "▼  " : "▶  ") + m_title);
}
