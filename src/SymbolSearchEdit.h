#pragma once

#include "SymbolSearchClient.h"

#include <QWidget>

class QComboBox;
class QTimer;

// Reusable widget: asset-type filter + editable live-search combo.
// Emits symbolConfirmed(symbol, name) when the user picks from the dropdown
// or presses Enter on a manually typed symbol (name will be empty in that case).
class SymbolSearchEdit : public QWidget {
    Q_OBJECT
public:
    explicit SymbolSearchEdit(QWidget* parent = nullptr);

    QString symbol()     const; // current ticker (trimmed, upper-cased)
    QString symbolName() const; // name from last dropdown selection, else ""
    void    setSymbol(const QString& sym);
    void    clear();

signals:
    void symbolConfirmed(const QString& symbol, const QString& name);

private slots:
    void onTextEdited(const QString& text);
    void onSearchTimerTimeout();
    void onSearchResults(const QList<SymbolResult>& results);
    void onItemActivated(int index);

private:
    QComboBox*          m_typeCombo;
    QComboBox*          m_symbolCombo;
    QTimer*             m_searchTimer;
    SymbolSearchClient* m_searchClient;
    bool                m_ignoreEdit = false;
    QString             m_lastName;
};
