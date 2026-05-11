#include "AiAssistantWidget.h"
#include "AiConfig.h"

#include <QCheckBox>
#include <QRegularExpression>
#include <QComboBox>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QMimeData>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

// ── plain-text-only input widget ─────────────────────────────────────────────

class PlainTextEdit : public QTextEdit {
public:
    explicit PlainTextEdit(QWidget* parent = nullptr) : QTextEdit(parent) {}
protected:
    void insertFromMimeData(const QMimeData* src) override {
        insertPlainText(src->text());
    }
};

// ── helpers ──────────────────────────────────────────────────────────────────

static QString htmlEscape(const QString& s)
{
    QString r = s;
    r.replace('&', "&amp;");
    r.replace('<', "&lt;");
    r.replace('>', "&gt;");
    r.replace('"', "&quot;");
    return r;
}

// Light markdown → HTML (bold, inline code, fenced code blocks, newlines)
QString AiAssistantWidget::fmtResponse(const QString& md)
{
    QString out;
    out.reserve(md.size() * 2);

    static const QRegularExpression reBold(R"(\*\*(.*?)\*\*)",
        QRegularExpression::InvertedGreedinessOption);
    static const QRegularExpression reIC(R"(`([^`]+)`)",
        QRegularExpression::InvertedGreedinessOption);

    auto inlineFormat = [&](QString chunk) -> QString {
        chunk.replace(reBold, "<b>\\1</b>");
        chunk.replace(reIC,
            "<code style='background:#f0f0f0;padding:1px 4px;"
            "font-family:monospace;font-size:12px'>\\1</code>");
        chunk.replace('\n', "<br>");
        return chunk;
    };

    const QString fence = "```";
    int pos = 0;
    while (pos < md.size()) {
        int fstart = md.indexOf(fence, pos);
        if (fstart < 0) {
            out += inlineFormat(md.mid(pos));
            break;
        }

        if (fstart > pos)
            out += inlineFormat(md.mid(pos, fstart - pos));

        int langEnd = md.indexOf('\n', fstart + 3);
        if (langEnd < 0) { pos = fstart + 3; break; }
        int codeStart = langEnd + 1;
        int fend = md.indexOf(fence, codeStart);

        if (fend < 0) {
            out += "<pre style='background:#1e1e1e;color:#d4d4d4;"
                   "padding:8px;font-family:monospace;font-size:12px'>"
                 + htmlEscape(md.mid(codeStart)) + "</pre>";
            pos = md.size();
            break;
        }

        out += "<pre style='background:#1e1e1e;color:#d4d4d4;"
               "padding:8px;font-family:monospace;font-size:12px'>"
             + htmlEscape(md.mid(codeStart, fend - codeStart)) + "</pre>";
        pos = fend + 3;
        if (pos < md.size() && md[pos] == '\n') ++pos;
    }
    return out;
}

// ── constructor ───────────────────────────────────────────────────────────────

AiAssistantWidget::AiAssistantWidget(QWidget* parent)
    : QWidget(parent)
{
    AiConfig::instance().load();

    // ── Top control bar ──────────────────────────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(6);

    topBar->addWidget(new QLabel("Provider:"));
    m_providerCombo = new QComboBox;
    for (const QString& name : AiConfig::instance().providerNames())
        m_providerCombo->addItem(name);
    topBar->addWidget(m_providerCombo);

    topBar->addWidget(new QLabel("Model:"));
    m_modelCombo = new QComboBox;
    m_modelCombo->setMinimumWidth(240);
    topBar->addWidget(m_modelCombo);

    topBar->addWidget(new QLabel("Max tokens:"));
    m_maxTokensCombo = new QComboBox;
    for (const char* s : {"512","1024","2048","4096","8192","16384","72768","max"})
        m_maxTokensCombo->addItem(s);
    m_maxTokensCombo->setCurrentText("4096");
    topBar->addWidget(m_maxTokensCombo);

    m_webSearchCheck = new QCheckBox("Web Search");
    topBar->addWidget(m_webSearchCheck);

    topBar->addStretch();

    m_configBtn = new QPushButton("Config");
    topBar->addWidget(m_configBtn);

    m_clearBtn = new QPushButton("Clear");
    topBar->addWidget(m_clearBtn);

    // ── Chat display ─────────────────────────────────────────────────────────
    m_chatDisplay = new QTextBrowser;
    m_chatDisplay->setOpenExternalLinks(true);
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setStyleSheet(
        "QTextBrowser { background:#FAFAFA; border:1px solid #D0D0D0; }");

    m_statusLbl = new QLabel;
    m_statusLbl->setStyleSheet("color:#888; font-style:italic; font-size:11px;");

    // ── Input area ───────────────────────────────────────────────────────────
    m_inputEdit = new PlainTextEdit;
    m_inputEdit->setPlaceholderText("Ask a question… (Ctrl+Enter to send)");
    m_inputEdit->setStyleSheet(
        "QTextEdit { background:#FFFFFF; color:#222222; border:1px solid #C0C0C0; }");
    {
        QPalette pal = m_inputEdit->palette();
        pal.setColor(QPalette::Base, Qt::white);
        pal.setColor(QPalette::Text, QColor(0x22, 0x22, 0x22));
        m_inputEdit->setPalette(pal);
    }
    int lineH = m_inputEdit->fontMetrics().lineSpacing();
    m_inputEdit->setMinimumHeight(lineH * 3 + 16);
    m_inputEdit->setMaximumHeight(lineH * 8 + 16);
    m_inputEdit->installEventFilter(this);

    m_sendBtn = new QPushButton("Send");
    m_sendBtn->setMinimumWidth(80);

    auto* inputRow = new QHBoxLayout;
    inputRow->addWidget(m_inputEdit);
    inputRow->addWidget(m_sendBtn, 0, Qt::AlignBottom);

    // ── Root layout ──────────────────────────────────────────────────────────
    auto* topGroup = new QGroupBox("AI Assistant");
    auto* gLayout  = new QVBoxLayout(topGroup);
    gLayout->setContentsMargins(6, 6, 6, 6);
    gLayout->setSpacing(6);
    gLayout->addLayout(topBar);
    gLayout->addWidget(m_chatDisplay, 1);
    gLayout->addWidget(m_statusLbl);
    gLayout->addLayout(inputRow);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->addWidget(topGroup, 1);

    // ── AI client ────────────────────────────────────────────────────────────
    m_client = new AiChatClient(this);
    connect(m_client, &AiChatClient::responseReady,
            this, &AiAssistantWidget::onResponseReady);
    connect(m_client, &AiChatClient::searchContextReady,
            this, &AiAssistantWidget::onSearchContext);
    connect(m_client, &AiChatClient::errorOccurred,
            this, &AiAssistantWidget::onError);
    connect(m_client, &AiChatClient::statusChanged,
            this, &AiAssistantWidget::onStatus);

    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AiAssistantWidget::onProviderChanged);
    connect(m_sendBtn,   &QPushButton::clicked, this, &AiAssistantWidget::onSend);
    connect(m_clearBtn,  &QPushButton::clicked, this, &AiAssistantWidget::onClear);
    connect(m_configBtn, &QPushButton::clicked, this, &AiAssistantWidget::onOpenConfig);

    onProviderChanged();
    appendSystem("Welcome! Select a provider and model, then start chatting.");
}

// ── event filter (Ctrl+Enter to send) ────────────────────────────────────────

bool AiAssistantWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_inputEdit && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && (ke->modifiers() & Qt::ControlModifier)) {
            onSend();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── model combo ──────────────────────────────────────────────────────────────

void AiAssistantWidget::populateModels()
{
    m_modelCombo->clear();
    const QString provider = m_providerCombo->currentText();
    const AiProviderConfig* cfg = AiConfig::instance().provider(provider);
    if (cfg) {
        for (const QString& m : cfg->models)
            m_modelCombo->addItem(m);
    }
}

void AiAssistantWidget::onProviderChanged()
{
    populateModels();
}

// ── send ─────────────────────────────────────────────────────────────────────

void AiAssistantWidget::onSend()
{
    const QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty() || m_client->isBusy()) return;

    m_inputEdit->clear();
    setInputEnabled(false);

    appendBubble("user", "You", text);

    // Pass a snapshot of history (without this message — client appends it internally)
    m_client->send(
        m_providerCombo->currentText(),
        m_modelCombo->currentText(),
        selectedMaxTokens(),
        m_history,
        text,
        m_webSearchCheck->isChecked()
    );

    m_history.append({"user", text});
}

// ── clear ─────────────────────────────────────────────────────────────────────

void AiAssistantWidget::onClear()
{
    m_chatHtml.clear();
    m_history.clear();
    m_chatDisplay->setHtml(
        "<html><body style='font-family:sans-serif;margin:4px;'></body></html>");
    appendSystem("Conversation cleared.");
}

// ── open config file ──────────────────────────────────────────────────────────

void AiAssistantWidget::onOpenConfig()
{
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(AiConfig::instance().configFilePath()));
}

// ── signals from client ───────────────────────────────────────────────────────

void AiAssistantWidget::onResponseReady(const QString& text)
{
    m_history.append({"assistant", text});
    appendBubble("assistant", "AI", text);
    setInputEnabled(true);
}

void AiAssistantWidget::onSearchContext(const QString& /*context*/)
{
    appendSystem("Web search results injected into context.");
}

void AiAssistantWidget::onError(const QString& msg)
{
    appendSystem("<span style='color:#cc0000'><b>Error:</b> "
                 + htmlEscape(msg) + "</span>");
    // Pop the user message we pre-added in onSend since no response came
    if (!m_history.isEmpty() && m_history.last().role == "user")
        m_history.removeLast();
    setInputEnabled(true);
}

void AiAssistantWidget::onStatus(const QString& status)
{
    m_statusLbl->setText(status);
}

// ── chat rendering ────────────────────────────────────────────────────────────

void AiAssistantWidget::appendBubble(const QString& role,
                                      const QString& label,
                                      const QString& text)
{
    const bool isUser = (role == "user");

    const QString bg        = isUser ? "#4a90d9"  : "#EFEFEF";
    const QString fg        = isUser ? "#FFFFFF"   : "#222222";
    const QString border    = isUser ? "#3a7abf"  : "#D0D0D0";
    const QString labelCol  = isUser ? "#c8e0ff"  : "#888888";
    const QString cellAlign = isUser ? "right"    : "left";

    const QString content = isUser
        ? htmlEscape(text).replace('\n', "<br>")
        : fmtResponse(text);

    m_chatHtml +=
        "<table width='100%' cellpadding='0' cellspacing='2' style='margin:4px 0;'>"
          "<tr><td align='" + cellAlign + "'>"
            "<table cellpadding='8' cellspacing='0' style='"
              "background:" + bg + ";"
              "color:" + fg + ";"
              "max-width:78%;"
              "border:1px solid " + border + ";'>"
              "<tr><td>"
                "<div style='font-size:10px;font-weight:bold;margin-bottom:4px;"
                "color:" + labelCol + ";'>"
                  + htmlEscape(label) +
                "</div>"
                "<div style='font-size:13px;line-height:1.6;'>"
                  + content +
                "</div>"
              "</td></tr>"
            "</table>"
          "</td></tr>"
        "</table>";

    refreshChat();
}

void AiAssistantWidget::appendSystem(const QString& text)
{
    m_chatHtml +=
        "<div style='text-align:center;color:#999;font-style:italic;"
        "font-size:11px;margin:6px 0;'>" + text + "</div>";
    refreshChat();
}

void AiAssistantWidget::scrollToBottom()
{
    QScrollBar* sb = m_chatDisplay->verticalScrollBar();
    sb->setValue(sb->maximum());
}

// ── private helpers ───────────────────────────────────────────────────────────

void AiAssistantWidget::refreshChat()
{
    m_chatDisplay->setHtml(
        "<html><body style='font-family:sans-serif;margin:6px;'>"
        + m_chatHtml + "</body></html>");
    scrollToBottom();
}

void AiAssistantWidget::setInputEnabled(bool on)
{
    m_inputEdit->setEnabled(on);
    m_sendBtn->setEnabled(on);
    m_providerCombo->setEnabled(on);
    m_modelCombo->setEnabled(on);
    m_maxTokensCombo->setEnabled(on);
    m_webSearchCheck->setEnabled(on);
    if (on) m_inputEdit->setFocus();
}

int AiAssistantWidget::selectedMaxTokens() const
{
    const QString t = m_maxTokensCombo->currentText();
    if (t == "max") return -1;
    bool ok = false;
    const int v = t.toInt(&ok);
    return ok ? v : 4096;
}
