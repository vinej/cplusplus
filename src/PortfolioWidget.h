#pragma once

#include "Candle.h"
#include "PortfolioDb.h"

#include <QMap>
#include <QWidget>
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class YahooFinanceClient;

class PortfolioWidget : public QWidget {
    Q_OBJECT
public:
    explicit PortfolioWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onPortfolioSelected(int index);
    void onNew();
    void onSave();
    void onSaveAs();
    void onDelete();
    void onChooseTicker();
    void onAddToPortfolio();
    void onRefreshPrices();
    void onEditSelected();
    void onRemoveSelected();
    void onRebalance();
    void openRebalanceDialog();
    void onSaveWeightAsOriginal();
    void onFetchFinished(const QString& symbol, const QString& tag, const CandleSeries& candles, const QString& name);
    void onFetchFailed  (const QString& symbol, const QString& tag, const QString& message);

private:
    void loadPortfolios();
    void clearInputs();
    void refreshGrid();
    void updateSummary();
    void markDirty();
    double computePositionPL(const PortfolioPosition& pos) const;
    double totalMarketValue() const;
    QMap<QString,double> currentWeights() const;

    // Network
    YahooFinanceClient* m_client;

    // Section 1 — portfolio selector
    QComboBox*   m_portfolioCombo;
    QPushButton* m_newBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_saveAsBtn;
    QPushButton* m_deleteBtn;

    // Section 2 — add / edit position
    QLineEdit*      m_tickerEdit;
    QLabel*         m_tickerNameLabel;
    QPushButton*    m_chooseTickerBtn;
    QDoubleSpinBox* m_qtySpin;
    QDoubleSpinBox* m_costSpin;
    QDateEdit*      m_dateEdit;
    QPushButton*    m_addBtn;

    // Section 3 — summary
    QLabel* m_totalValueLabel;
    QLabel* m_totalPlLabel;

    // Section 4 — grid actions
    QPushButton* m_refreshBtn;
    QPushButton* m_editBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_rebalanceBtn;
    QPushButton* m_saveWeightBtn;

    // Section 5 — grid
    QTableWidget* m_grid;

    // State
    int  m_currentPortfolioId          = -1;
    bool m_dirty                       = false;
    bool m_openRebalanceAfterRefresh   = false;
    int m_pendingRefreshCount  = 0;
    QVector<PortfolioPosition>         m_positions;
    QMap<QString,double>               m_currentPrices;
    QMap<QString,QMap<QDate,double>>   m_adjHistory;   // symbol → date → adjClose

    // Inline edit state
    bool m_editingExisting    = false;
    int  m_editingPositionIdx = -1;
};
