#pragma once

#include <QDateTime>
#include <QVector>

// One bar of OHLCV market data. Trivially copyable so we pass by value freely.
struct Candle {
    QDateTime timestamp;
    double open     = 0.0;
    double high     = 0.0;
    double low      = 0.0;
    double close    = 0.0;
    double adjClose = 0.0; // split- and dividend-adjusted close for return calculations
    qint64 volume   = 0;
};

using CandleSeries = QVector<Candle>;
