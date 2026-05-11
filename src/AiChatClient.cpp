#include "AiChatClient.h"
#include "AiConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

AiChatClient::AiChatClient(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{}

void AiChatClient::abort()
{
    if (m_reply) { m_reply->abort(); m_reply = nullptr; }
    m_busy = false;
    emit statusChanged({});
}

void AiChatClient::send(const QString&          providerName,
                         const QString&          model,
                         int                     maxTokens,
                         const QVector<ChatMessage>& history,
                         const QString&          userMessage,
                         bool                    webSearch)
{
    if (m_busy) return;
    m_busy      = true;
    m_provider  = providerName;
    m_model     = model;
    m_maxTokens = maxTokens;
    m_messages  = history;
    m_messages.append({"user", userMessage});

    if (webSearch && !AiConfig::instance().webSearch().apiKey.isEmpty()) {
        emit statusChanged("Searching the web…");
        doSearch(userMessage);
    } else {
        emit statusChanged("Thinking…");
        doChat();
    }
}

// ── Web search (Brave Search API) ─────────────────────────────────────────────

void AiChatClient::doSearch(const QString& query)
{
    const auto& ws = AiConfig::instance().webSearch();

    QUrl url(ws.baseUrl);
    QUrlQuery q;
    q.addQueryItem("q",     query);
    q.addQueryItem("count", QString::number(ws.maxResults));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Accept",               "application/json");
    req.setRawHeader("X-Subscription-Token", ws.apiKey.toUtf8());

    m_reply = m_net->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &AiChatClient::onSearchReply);
}

void AiChatClient::onSearchReply()
{
    if (!m_reply) return;
    const QByteArray data = m_reply->readAll();
    m_reply->deleteLater();
    m_reply = nullptr;

    QString context;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        const QJsonArray results =
            doc.object().value("web").toObject().value("results").toArray();
        if (!results.isEmpty()) {
            context = "## Web Search Results\n\n";
            const int limit = qMin(results.size(),
                                   AiConfig::instance().webSearch().maxResults);
            for (int i = 0; i < limit; ++i) {
                const QJsonObject r = results[i].toObject();
                context += QString("%1. **%2**\n   %3\n   %4\n\n")
                    .arg(i + 1)
                    .arg(r.value("title").toString())
                    .arg(r.value("description").toString())
                    .arg(r.value("url").toString());
            }
            context += "---\n\n";

            // Prepend the search context to the last user message
            if (!m_messages.isEmpty() && m_messages.last().role == "user")
                m_messages.last().content = context + m_messages.last().content;

            emit searchContextReady(context);
        }
    }

    emit statusChanged("Thinking…");
    doChat();
}

// ── Chat dispatch ─────────────────────────────────────────────────────────────

void AiChatClient::doChat()
{
    const AiProviderConfig* cfg = AiConfig::instance().provider(m_provider);
    if (!cfg) {
        emit errorOccurred("Unknown provider: " + m_provider);
        m_busy = false;
        emit statusChanged({});
        return;
    }
    if (cfg->apiKey.isEmpty() && m_provider != "Ollama") {
        emit errorOccurred(m_provider + " API key is not set — edit ai_config.json.");
        m_busy = false;
        emit statusChanged({});
        return;
    }

    QNetworkRequest req;
    QByteArray      body;

    if (m_provider == "Anthropic") {
        // ── Anthropic Messages API ─────────────────────────────────────────────
        req.setUrl(QUrl(cfg->baseUrl + "/messages"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("x-api-key",          cfg->apiKey.toUtf8());
        req.setRawHeader("anthropic-version",  "2023-06-01");

        QJsonArray msgs;
        QString    sysMsg;
        for (const auto& m : m_messages) {
            if (m.role == "system") { sysMsg = m.content; continue; }
            QJsonObject o;
            o["role"]    = m.role;
            o["content"] = m.content;
            msgs.append(o);
        }

        QJsonObject j;
        j["model"]      = m_model;
        j["messages"]   = msgs;
        j["max_tokens"] = (m_maxTokens > 0) ? m_maxTokens : 8192;  // required field
        if (!sysMsg.isEmpty()) j["system"] = sysMsg;

        body = QJsonDocument(j).toJson(QJsonDocument::Compact);

    } else if (m_provider == "Gemini") {
        // ── Gemini generateContent API ─────────────────────────────────────────
        req.setUrl(QUrl(cfg->baseUrl + "/models/" + m_model +
                        ":generateContent?key=" + cfg->apiKey));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonArray contents;
        for (const auto& m : m_messages) {
            if (m.role == "system") continue;   // handled via systemInstruction below
            QJsonObject part;  part["text"] = m.content;
            QJsonObject o;
            o["role"]  = (m.role == "assistant") ? QString("model") : QString("user");
            o["parts"] = QJsonArray{part};
            contents.append(o);
        }

        QJsonObject j;
        j["contents"] = contents;

        // System message
        for (const auto& m : m_messages) {
            if (m.role == "system") {
                QJsonObject part;  part["text"] = m.content;
                QJsonObject si;    si["parts"]  = QJsonArray{part};
                j["systemInstruction"] = si;
                break;
            }
        }

        if (m_maxTokens > 0) {
            QJsonObject gc;  gc["maxOutputTokens"] = m_maxTokens;
            j["generationConfig"] = gc;
        }

        body = QJsonDocument(j).toJson(QJsonDocument::Compact);

    } else {
        // ── OpenAI-compatible: OpenAI, Groq, OpenRouter, Ollama ───────────────
        req.setUrl(QUrl(cfg->baseUrl + "/chat/completions"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!cfg->apiKey.isEmpty())
            req.setRawHeader("Authorization", ("Bearer " + cfg->apiKey).toUtf8());
        if (m_provider == "OpenRouter") {
            req.setRawHeader("HTTP-Referer", "https://qt-finance-app");
            req.setRawHeader("X-Title",      "Qt Finance");
        }

        QJsonArray msgs;
        for (const auto& m : m_messages) {
            QJsonObject o;  o["role"] = m.role;  o["content"] = m.content;
            msgs.append(o);
        }

        QJsonObject j;
        j["model"]    = m_model;
        j["messages"] = msgs;
        if (m_maxTokens > 0) j["max_tokens"] = m_maxTokens;

        body = QJsonDocument(j).toJson(QJsonDocument::Compact);
    }

    m_reply = m_net->post(req, body);
    connect(m_reply, &QNetworkReply::finished, this, &AiChatClient::onChatReply);
}

void AiChatClient::onChatReply()
{
    if (!m_reply) return;
    m_busy = false;
    emit statusChanged({});

    const int        httpCode = m_reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data     = m_reply->readAll();
    m_reply->deleteLater();
    m_reply = nullptr;

    // Parse error body for a helpful message
    if (httpCode < 200 || httpCode >= 300) {
        QString msg = QString("HTTP %1").arg(httpCode);
        const QJsonDocument ed = QJsonDocument::fromJson(data);
        if (ed.isObject()) {
            const auto err = ed.object().value("error");
            if (err.isObject())      msg = err.toObject().value("message").toString();
            else if (err.isString()) msg = err.toString();
        }
        emit errorOccurred(msg);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) { emit errorOccurred("Invalid JSON response"); return; }
    const QJsonObject obj = doc.object();

    QString text;
    if (m_provider == "Anthropic") {
        for (const auto& c : obj["content"].toArray())
            if (c.toObject()["type"].toString() == "text")
                text += c.toObject()["text"].toString();
    } else if (m_provider == "Gemini") {
        const QJsonArray cands = obj["candidates"].toArray();
        if (!cands.isEmpty()) {
            for (const auto& p : cands[0].toObject()["content"]
                                         .toObject()["parts"].toArray())
                text += p.toObject()["text"].toString();
        }
    } else {
        const QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty())
            text = choices[0].toObject()["message"].toObject()["content"].toString();
    }

    if (text.isEmpty())
        emit errorOccurred("Empty response from " + m_provider);
    else
        emit responseReady(text);
}
