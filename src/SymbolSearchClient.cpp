#include "SymbolSearchClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

SymbolSearchClient::SymbolSearchClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

void SymbolSearchClient::search(const QString& query, const QString& typeFilter)
{
    cancel();
    if (query.trimmed().isEmpty()) {
        emit resultsReady({});
        return;
    }
    m_pendingType = typeFilter;

    // Request more results when filtering by a specific type so that
    // lower-ranked items (e.g. ^GSPC for query "500") survive the client-side filter.
    const int quotesCount = (typeFilter == "All") ? 20 : 50;

    QUrl url("https://query1.finance.yahoo.com/v1/finance/search");
    QUrlQuery q;
    q.addQueryItem("q",           query.trimmed());
    q.addQueryItem("quotesCount", QString::number(quotesCount));
    q.addQueryItem("newsCount",   "0");
    q.addQueryItem("listsCount",  "0");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    m_pending = m_nam->get(req);
    connect(m_pending, &QNetworkReply::finished,
            this, &SymbolSearchClient::onReplyFinished);
}

void SymbolSearchClient::cancel()
{
    if (!m_pending) return;
    m_pending->disconnect(this); // prevent onReplyFinished from firing on abort
    m_pending->abort();
    m_pending->deleteLater();
    m_pending = nullptr;
}

void SymbolSearchClient::onReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || reply != m_pending) {
        if (reply) reply->deleteLater();
        return;
    }
    m_pending = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) return;
    if (reply->error() != QNetworkReply::NoError) {
        emit resultsReady({});
        return;
    }

    const QJsonDocument doc    = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray    quotes = doc.object().value("quotes").toArray();

    QList<SymbolResult> results;
    for (const QJsonValue& v : quotes) {
        const QJsonObject o   = v.toObject();
        const QString     sym = o.value("symbol").toString();
        if (sym.isEmpty()) continue;

        const QString qt = o.value("quoteType").toString();
        // Yahoo's search endpoint misclassifies ^-prefixed index symbols as
        // EQUITY; use the ^ prefix as the authoritative indicator instead.
        const QString mapped = sym.startsWith('^') ? QStringLiteral("Index")
                                                    : mapQuoteType(qt);
        if (mapped.isEmpty()) continue;
        if (m_pendingType != "All" && mapped != m_pendingType) continue;

        SymbolResult r;
        r.symbol = sym;
        r.name   = o.value("longname").toString();
        if (r.name.isEmpty()) r.name = o.value("shortname").toString();
        r.type   = mapped;
        results.append(r);
    }
    emit resultsReady(results);
}

QString SymbolSearchClient::mapQuoteType(const QString& qt)
{
    if (qt == "EQUITY")         return "Stock";
    if (qt == "ETF")            return "ETF";
    if (qt == "INDEX")          return "Index";
    if (qt == "FUTURE")         return "Commodity";
    if (qt == "CRYPTOCURRENCY") return "Crypto";
    if (qt == "MUTUALFUND")     return "Mutual Fund";
    return {};
}
