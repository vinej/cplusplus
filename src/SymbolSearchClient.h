#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct SymbolResult {
    QString symbol;
    QString name;
    QString type; // "Stock", "ETF", "Index", "Commodity", "Crypto", "Mutual Fund"
};
Q_DECLARE_METATYPE(SymbolResult)

class QNetworkAccessManager;
class QNetworkReply;

class SymbolSearchClient : public QObject {
    Q_OBJECT
public:
    explicit SymbolSearchClient(QObject* parent = nullptr);

    void search(const QString& query, const QString& typeFilter = "All");
    void cancel();

signals:
    void resultsReady(const QList<SymbolResult>& results);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager* m_nam;
    QNetworkReply*         m_pending  = nullptr;
    QString                m_pendingType;

    static QString mapQuoteType(const QString& qt);
};
