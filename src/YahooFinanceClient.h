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
    // interval examples: "1d" (daily), "1mo" (monthly)
    void fetchDaily(const QString& symbol,
                    const QString& range    = QStringLiteral("1y"),
                    const QString& interval = QStringLiteral("1d"));

    // Fetch using explicit Unix-epoch timestamps instead of a named range.
    // type = "history" → emits historyReady(); type = "max" → emits maxReady().
    void fetchByPeriod(const QString& symbol, qint64 period1, qint64 period2,
                       const QString& interval = QStringLiteral("1d"),
                       const QString& type     = QStringLiteral("history"));

signals:
    void finished(const QString& symbol, const CandleSeries& candles);
    void historyReady(const QString& symbol, const CandleSeries& candles);
    void maxReady(const QString& symbol, const CandleSeries& candles);
    void failed(const QString& symbol, const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_net;
};
