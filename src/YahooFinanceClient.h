#pragma once

#include "Candle.h"

#include <QDate>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

// Smart Yahoo Finance client.
//
// Known per-interval data limits (Yahoo Finance v8 chart endpoint):
//   1m          →  7 days of history
//   2m/5m/15m/30m/90m → 59 days
//   60m/1h      → 729 days
//   1d/1wk/1mo/3mo → full history (chunked in 20-year windows for reliability)
//
// fetch() always accepts (interval, startDate, endDate) or (interval, startDate, range).
// It transparently splits the request into sequential chunks when needed,
// merges the results, and emits finished() once with the complete sorted series.
// tag is echoed back in finished()/failed() for caller-side routing.
class YahooFinanceClient : public QObject {
    Q_OBJECT
public:
    explicit YahooFinanceClient(QObject* parent = nullptr);

    // Fetch [startDate, endDate] bars for interval.
    // Intraday: startDate is silently clamped to Yahoo's max history window.
    // endDate defaults to today when invalid.
    void fetch(const QString& symbol,
               const QString& interval,
               const QDate&   startDate,
               const QDate&   endDate = QDate(),
               const QString& tag     = QString());

    // Convenience: endDate = startDate + range duration, capped at today.
    // range: "1d" "5d" "1mo" "3mo" "6mo" "1y" "2y" "5y" "10y" "ytd" "max"
    void fetch(const QString& symbol,
               const QString& interval,
               const QDate&   startDate,
               const QString& range,
               const QString& tag = QString());

signals:
    void finished(const QString& symbol, const QString& tag, const CandleSeries& candles);
    void failed  (const QString& symbol, const QString& tag, const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    struct Job {
        QString      symbol;
        QString      interval;
        QString      tag;
        QVector<QPair<qint64,qint64>> pending;   // remaining (period1,period2) pairs
        CandleSeries accumulated;
    };

    // Max days per single Yahoo request for interval.
    // For intraday this equals Yahoo's total available history window.
    static int  maxDaysPerChunk(const QString& interval);
    // True for intervals whose Yahoo history window is bounded (intraday).
    static bool isIntraday(const QString& interval);
    // Compute endDate = startDate + range, capped at today.
    static QDate rangeEndDate(const QDate& startDate, const QString& range);
    // Build ordered (period1,period2) UTC epoch pairs covering [start,end].
    static QVector<QPair<qint64,qint64>> buildChunks(const QDate& start,
                                                       const QDate& end,
                                                       int maxDays);
    CandleSeries parseCandles(const QByteArray& body, QString& outError) const;
    void fireNext(const QString& jobId);

    QNetworkAccessManager* m_net;
    QMap<QString, Job>     m_jobs;
    int                    m_nextId = 0;
};
