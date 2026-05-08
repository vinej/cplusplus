#include "Indicators.h"

#include <ta-lib/ta_libc.h>

#include <cmath>
#include <limits>

namespace {

struct TaLibInit {
    TaLibInit()  { TA_Initialize(); }
    ~TaLibInit() { TA_Shutdown();   }
};
// Process-wide one-shot init/shutdown — TA-Lib requires this around any call.
const TaLibInit g_taLibInit;

QVector<double> runSingleOutput(
    const QVector<double>& in,
    int period,
    TA_RetCode (*fn)(int, int, const double*, int, int*, int*, double*))
{
    const int n = in.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (n == 0 || period <= 0) return out;

    int outBeg = 0;
    int outNb  = 0;
    QVector<double> raw(n);
    const TA_RetCode rc = fn(0, n - 1, in.constData(), period, &outBeg, &outNb, raw.data());
    if (rc != TA_SUCCESS) return out;

    for (int i = 0; i < outNb; ++i) {
        out[outBeg + i] = raw[i];
    }
    return out;
}

} // namespace

namespace Indicators {

QVector<double> sma(const QVector<double>& close, int period) {
    return runSingleOutput(close, period, &TA_SMA);
}

QVector<double> ema(const QVector<double>& close, int period) {
    return runSingleOutput(close, period, &TA_EMA);
}

QVector<double> rsi(const QVector<double>& close, int period) {
    return runSingleOutput(close, period, &TA_RSI);
}

} // namespace Indicators
