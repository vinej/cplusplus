#pragma once

#include <QFrame>

class QPushButton;

class CollapsiblePanel : public QFrame {
    Q_OBJECT
public:
    explicit CollapsiblePanel(const QString& title, QWidget* parent = nullptr);
    QWidget* body() const;

private slots:
    void onToggle();

private:
    QPushButton* m_header;
    QWidget*     m_body;
    QString      m_title;
    bool         m_expanded = true;
};
