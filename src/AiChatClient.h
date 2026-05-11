#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

struct ChatMessage {
    QString role;     // "user"  |  "assistant"  |  "system"
    QString content;
};

class AiChatClient : public QObject {
    Q_OBJECT
public:
    explicit AiChatClient(QObject* parent = nullptr);

    // maxTokens == -1  →  let the provider use its maximum
    void send(const QString&          providerName,
              const QString&          model,
              int                     maxTokens,
              const QVector<ChatMessage>& history,
              const QString&          userMessage,
              bool                    webSearch = false);

    void abort();
    bool isBusy() const { return m_busy; }

signals:
    void searchContextReady(const QString& context);   // web search context injected
    void responseReady(const QString& text);
    void errorOccurred(const QString& errorMessage);
    void statusChanged(const QString& status);         // "" means idle

private slots:
    void onSearchReply();
    void onChatReply();

private:
    void doSearch(const QString& query);
    void doChat();

    QNetworkAccessManager* m_net;

    bool                 m_busy      = false;
    QString              m_provider;
    QString              m_model;
    int                  m_maxTokens = 4096;
    QVector<ChatMessage> m_messages;      // history + current user msg (possibly augmented)
    QNetworkReply*       m_reply     = nullptr;
};
