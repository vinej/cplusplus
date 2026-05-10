#pragma once

#include "PortfolioDb.h"

#include <QDialog>
#include <QMap>
#include <QVector>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;

class RebalanceDialog : public QDialog {
    Q_OBJECT
public:
    RebalanceDialog(const QVector<PortfolioPosition>& positions,
                    const QMap<QString,double>& currentPrices,
                    QWidget* parent = nullptr);

    QMap<QString,double> newQuantities() const;

private:
    void updateRow(int i);
    void updateSum();

    QDoubleSpinBox*          m_totalSpin;
    QTableWidget*            m_grid;
    QVector<QDoubleSpinBox*> m_weightSpins;
    QLabel*                  m_sumLabel;
    QPushButton*             m_applyBtn;

    QVector<PortfolioPosition> m_positions;
    QMap<QString,double>       m_currentPrices;
};
