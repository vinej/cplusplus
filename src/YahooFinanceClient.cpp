#include "YahooFinanceClient.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

#include <nlohmann/json.hpp>

using nlohmann::json;

// ── ctor ──────────────────────────────────────────────────────────────────────

YahooFinanceClient::YahooFinanceClient(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    connect(m_net, &QNetworkAccessManager::finished,
            this,  &YahooFinanceClient::onReplyFinished);
}

// ── static helpers ────────────────────────────────────────────────────────────

int YahooFinanceClient::maxDaysPerChunk(const QString& interval)
{
    if (interval == "1m")                              return 7;
    if (interval == "2m"  || interval == "5m"  ||
        interval == "15m" || interval == "30m" ||
        interval == "90m")                             return 59;
    if (interval == "60m" || interval == "1h")         return 729;
    return 7300; // daily and above: no practical limit — use 20-year chunks
}

bool YahooFinanceClient::isIntraday(const QString& interval)
{
    return maxDaysPerChunk(interval) < 3650;
}

QDate YahooFinanceClient::rangeEndDate(const QDate& start, const QString& range)
{
    const QDate today = QDate::currentDate();
    QDate end;
    if      (range == "1d")  end = start.addDays(1);
    else if (range == "5d")  end = start.addDays(5);
    else if (range == "1mo") end = start.addMonths(1);
    else if (range == "3mo") end = start.addMonths(3);
    else if (range == "6mo") end = start.addMonths(6);
    else if (range == "1y")  end = start.addYears(1);
    else if (range == "2y")  end = start.addYears(2);
    else if (range == "5y")  end = start.addYears(5);
    else if (range == "10y") end = start.addYears(10);
    else if (range == "ytd") end = QDate(today.year(), 12, 31);
    else                     end = today; // "max" or unknown → up to today
    return qMin(end, today);
}

QVector<QPair<qint64,qint64>>
YahooFinanceClient::buildChunks(const QDate& start, const QDate& end, int maxDays)
{
    QVector<QPair<qint64,qint64>> chunks;
    QDate cur = start;
    while (cur <= end) {
        const QDate chunkEnd = qMin(cur.addDays(maxDays - 1), end);
        chunks.append({
            QDateTime(cur,      QTime(0, 0, 0),  Qt::UTC).toSecsSinceEpoch(),
            QDateTime(chunkEnd, QTime(23,59,59), Qt::UTC).toSecsSinceEpoch()
        });
        cur = chunkEnd.addDays(1);
    }
    return chunks;
}

// ── public API ────────────────────────────────────────────────────────────────

void YahooFinanceClient::fetch(const QString& symbol,
                                const QString& interval,
                                const QDate&   startDate,
                                const QDate&   endDate,
                                const QString& tag)
{
    const QDate today   = QDate::currentDate();
    const int   maxDays = maxDaysPerChunk(interval);

    // Intraday: Yahoo only stores maxDays of history — clamp start to that window.
    QDate effectiveStart = isIntraday(interval)
        ? qMax(startDate, today.addDays(-(maxDays - 1)))
        : startDate;
    QDate effectiveEnd = (endDate.isValid() && endDate <= today) ? endDate : today;

    if (effectiveStart > effectiveEnd) {
        emit finished(symbol, tag, CandleSeries{}, QString());
        return;
    }

    Job job;
    job.symbol   = symbol;
    job.interval = interval;
    job.tag      = tag;
    job.pending  = buildChunks(effectiveStart, effectiveEnd, maxDays);

    const QString jobId = QString::number(m_nextId++);
    m_jobs.insert(jobId, std::move(job));
    fireNext(jobId);
}

void YahooFinanceClient::fetch(const QString& symbol,
                                const QString& interval,
                                const QDate&   startDate,
                                const QString& range,
                                const QString& tag)
{
    fetch(symbol, interval, startDate, rangeEndDate(startDate, range), tag);
}

// ── network ───────────────────────────────────────────────────────────────────

void YahooFinanceClient::fireNext(const QString& jobId)
{
    const Job& job = m_jobs.value(jobId);
    const auto& chunk = job.pending.first();

    QUrl url(QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/%1")
                 .arg(job.symbol));
    QUrlQuery q;
    q.addQueryItem("period1",  QString::number(chunk.first));
    q.addQueryItem("period2",  QString::number(chunk.second));
    q.addQueryItem("interval", job.interval);
    q.addQueryItem("events",   "div,splits");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (qt-finance learning project)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::User, jobId);
    m_net->get(req);
}

CandleSeries YahooFinanceClient::parseCandles(const QByteArray& body,
                                               QString& outError,
                                               QString& outName) const
{
    json doc;
    try {
        doc = json::parse(body.constData(), body.constData() + body.size());
    } catch (const std::exception& e) {
        outError = QString("JSON parse error: %1").arg(e.what());
        return {};
    }

    try {
        const auto& result = doc.at("chart").at("result");
        if (!result.is_array() || result.empty())
            return {}; // no data for this period — not an error

        const auto& r0         = result.at(0);

        // Extract display name from meta (shortName preferred over longName).
        if (r0.contains("meta")) {
            const auto& meta = r0.at("meta");
            if (meta.contains("shortName") && meta["shortName"].is_string())
                outName = QString::fromStdString(meta["shortName"].get<std::string>());
            else if (meta.contains("longName") && meta["longName"].is_string())
                outName = QString::fromStdString(meta["longName"].get<std::string>());
        }

        const auto& timestamps = r0.at("timestamp");
        const auto& indicators = r0.at("indicators");
        const auto& quote      = indicators.at("quote").at(0);
        const auto& opens      = quote.at("open");
        const auto& highs      = quote.at("high");
        const auto& lows       = quote.at("low");
        const auto& closes     = quote.at("close");
        const auto& volumes    = quote.at("volume");

        const bool  hasAdj    = indicators.contains("adjclose") &&
                                indicators.at("adjclose").is_array() &&
                                !indicators.at("adjclose").empty();
        const json* adjCloses = hasAdj
            ? &indicators.at("adjclose").at(0).at("adjclose")
            : nullptr;

        CandleSeries out;
        out.reserve(static_cast<int>(timestamps.size()));

        for (std::size_t i = 0; i < timestamps.size(); ++i) {
            if (opens[i].is_null() || highs[i].is_null() ||
                lows[i].is_null()  || closes[i].is_null()) continue;
            Candle c;
            c.timestamp = QDateTime::fromSecsSinceEpoch(
                timestamps[i].get<qint64>(), Qt::UTC);
            c.open     = opens[i].get<double>();
            c.high     = highs[i].get<double>();
            c.low      = lows[i].get<double>();
            c.close    = closes[i].get<double>();
            c.adjClose = (adjCloses && !(*adjCloses)[i].is_null())
                             ? (*adjCloses)[i].get<double>()
                             : c.close;
            c.volume   = volumes[i].is_null() ? 0 : volumes[i].get<qint64>();
            out.append(c);
        }
        return out;

    } catch (const std::exception& e) {
        outError = QString("Unexpected JSON shape: %1").arg(e.what());
        return {};
    }
}

void YahooFinanceClient::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    const QString jobId = reply->request().attribute(QNetworkRequest::User).toString();

    if (!m_jobs.contains(jobId)) return; // stale reply

    Job& job = m_jobs[jobId];

    if (reply->error() != QNetworkReply::NoError) {
        const QString sym = job.symbol, tag = job.tag;
        m_jobs.remove(jobId);
        emit failed(sym, tag, reply->errorString());
        return;
    }

    QString err, name;
    const CandleSeries chunk = parseCandles(reply->readAll(), err, name);
    if (!err.isEmpty()) {
        const QString sym = job.symbol, tag = job.tag;
        m_jobs.remove(jobId);
        emit failed(sym, tag, err);
        return;
    }

    if (job.name.isEmpty()) job.name = name; // first chunk with a name wins
    job.accumulated.append(chunk);
    job.pending.removeFirst();

    if (!job.pending.isEmpty()) {
        fireNext(jobId);
        return;
    }

    // All chunks done — sort chronologically and deduplicate by date.
    auto& acc = job.accumulated;
    std::sort(acc.begin(), acc.end(), [](const Candle& a, const Candle& b) {
        return a.timestamp.toSecsSinceEpoch() < b.timestamp.toSecsSinceEpoch();
    });
    int w = 0;
    for (int i = 0; i < acc.size(); ++i) {
        if (w == 0 || acc[i].timestamp.date() != acc[w-1].timestamp.date())
            acc[w++] = acc[i];
    }
    acc.resize(w);

    const QString sym = job.symbol, tag = job.tag, jobName = job.name;
    CandleSeries result = std::move(job.accumulated);
    m_jobs.remove(jobId);
    emit finished(sym, tag, result, jobName);
}
