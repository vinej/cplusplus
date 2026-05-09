#include "YahooFinanceClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <nlohmann/json.hpp>

using nlohmann::json;

YahooFinanceClient::YahooFinanceClient(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    connect(m_net, &QNetworkAccessManager::finished,
            this,  &YahooFinanceClient::onReplyFinished);
}

void YahooFinanceClient::fetchDaily(const QString& symbol,
                                    const QString& range,
                                    const QString& interval)
{
    QUrl url(QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/%1").arg(symbol));
    QUrlQuery q;
    q.addQueryItem("range",    range);
    q.addQueryItem("interval", interval);
    q.addQueryItem("events",   "div,splits");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (qt-finance learning project)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::User, symbol + "|chart|" + interval);
    m_net->get(req);
}

void YahooFinanceClient::fetchByPeriod(const QString& symbol,
                                        qint64 period1, qint64 period2,
                                        const QString& interval,
                                        const QString& type)
{
    QUrl url(QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/%1").arg(symbol));
    QUrlQuery q;
    q.addQueryItem("period1",  QString::number(period1));
    q.addQueryItem("period2",  QString::number(period2));
    q.addQueryItem("interval", interval);
    q.addQueryItem("events",   "div,splits");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (qt-finance learning project)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::User, symbol + "|" + type + "|" + interval);
    m_net->get(req);
}

void YahooFinanceClient::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    const QString tag      = reply->request().attribute(QNetworkRequest::User).toString();
    const QString symbol   = tag.section('|', 0, 0);
    const QString type     = tag.section('|', 1, 1); // "chart" or "history"
    const QString interval = tag.section('|', 2, 2);

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(symbol, reply->errorString());
        return;
    }

    const QByteArray body = reply->readAll();
    json doc;
    try {
        doc = json::parse(body.constData(), body.constData() + body.size());
    } catch (const std::exception& e) {
        emit failed(symbol, QString("JSON parse error: %1").arg(e.what()));
        return;
    }

    try {
        const auto& result = doc.at("chart").at("result");
        if (!result.is_array() || result.empty()) {
            emit failed(symbol, "Empty result from Yahoo");
            return;
        }

        const auto& r0         = result.at(0);
        const auto& timestamps = r0.at("timestamp");
        const auto& indicators = r0.at("indicators");
        const auto& quote      = indicators.at("quote").at(0);
        const auto& opens      = quote.at("open");
        const auto& highs      = quote.at("high");
        const auto& lows       = quote.at("low");
        const auto& closes     = quote.at("close");
        const auto& volumes    = quote.at("volume");

        // adjclose is present for equities; may be absent for some instruments.
        const bool hasAdj = indicators.contains("adjclose") &&
                            indicators.at("adjclose").is_array() &&
                            !indicators.at("adjclose").empty();
        const json* adjCloses = hasAdj
            ? &indicators.at("adjclose").at(0).at("adjclose")
            : nullptr;

        CandleSeries out;
        out.reserve(static_cast<int>(timestamps.size()));

        for (std::size_t i = 0; i < timestamps.size(); ++i) {
            // Yahoo emits null for missing bars (e.g. trading halts) — skip them.
            if (opens[i].is_null() || highs[i].is_null() ||
                lows[i].is_null()  || closes[i].is_null()) {
                continue;
            }
            Candle c;
            c.timestamp = QDateTime::fromSecsSinceEpoch(timestamps[i].get<qint64>(), Qt::UTC);
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

        if (type == "history")
            emit historyReady(symbol, out);
        else if (type == "max")
            emit maxReady(symbol, out);
        else
            emit finished(symbol, out);
    } catch (const std::exception& e) {
        emit failed(symbol, QString("Unexpected JSON shape: %1").arg(e.what()));
    }
}
