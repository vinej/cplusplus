#pragma once

#include "AiChatClient.h"
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTextBrowser;
class QTextEdit;

class AiAssistantWidget : public QWidget {
    Q_OBJECT
public:
    explicit AiAssistantWidget(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onProviderChanged();
    void onSend();
    void onClear();
    void onOpenConfig();
    void onResponseReady(const QString& text);
    void onSearchContext(const QString& context);
    void onError(const QString& msg);
    void onStatus(const QString& status);

private:
    void   populateModels();
    void   appendBubble(const QString& role, const QString& label, const QString& text);
    void   appendSystem(const QString& text);
    void   scrollToBottom();
    void   refreshChat();
    void   setInputEnabled(bool on);
    int    selectedMaxTokens() const;
    static QString fmtResponse(const QString& md);

    // Controls
    QComboBox*   m_providerCombo  = nullptr;
    QComboBox*   m_modelCombo     = nullptr;
    QComboBox*   m_maxTokensCombo = nullptr;
    QCheckBox*   m_webSearchCheck = nullptr;
    QPushButton* m_configBtn      = nullptr;
    QPushButton* m_clearBtn       = nullptr;

    // Chat display
    QTextBrowser* m_chatDisplay   = nullptr;
    QLabel*       m_statusLbl     = nullptr;

    // Input
    QTextEdit*   m_inputEdit      = nullptr;
    QPushButton* m_sendBtn        = nullptr;

    // State
    AiChatClient*        m_client  = nullptr;
    QVector<ChatMessage> m_history;
    QString              m_chatHtml;
};
