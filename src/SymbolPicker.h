#pragma once

#include <QDate>
#include <QWidget>

class SymbolSearchEdit;
class QComboBox;
class QPushButton;

class SymbolPicker : public QWidget {
    Q_OBJECT
public:
    explicit SymbolPicker(QWidget* parent = nullptr);

    QString symbol()    const;
    QString range()     const;
    QString interval()  const;
    QDate   startDate() const; // converts current range to an absolute start date

    void setFetchEnabled(bool enabled);

signals:
    void fetchRequested();
    void rangeChanged(const QString& range);
    void intervalChanged(const QString& interval);

private:
    SymbolSearchEdit* m_searchEdit;
    QComboBox*        m_rangeCombo;
    QComboBox*        m_intervalCombo;
    QPushButton*      m_fetchButton;
};
