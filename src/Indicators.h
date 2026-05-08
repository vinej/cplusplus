#pragma once

#include <QVector>

// Thin wrappers over TA-Lib. Each function returns a vector aligned with the
// input series — leading values that TA-Lib can't compute (warm-up window) are
// filled with NaN so chart code can skip them.
namespace Indicators {

QVector<double> sma(const QVector<double>& close, int period);
QVector<double> ema(const QVector<double>& close, int period);
QVector<double> rsi(const QVector<double>& close, int period);

} // namespace Indicators
