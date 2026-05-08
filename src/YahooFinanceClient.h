#pragma once

#include "Candle.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Fetches daily OHLCV bars from Yahoo Finance's public chart endpoint.
// Network I/O is async — call fetchDaily() and listen for finished()/failed().
class YahooFinanceClient : public QObject {
    Q_OBJECT
public:
    explicit YahooFinanceClient(QObject* parent = nullptr);

    // range examples: "1mo", "3mo", "6mo", "1y", "5y", "max"
    void fetchDaily(const QString& symbol, const QString& range = QStringLiteral("1y"));

signals:
    void finished(const QString& symbol, const CandleSeries& candles);
    void failed(const QString& symbol, const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_net;
};
