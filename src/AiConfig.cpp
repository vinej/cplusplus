#include "AiConfig.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;

AiConfig& AiConfig::instance()
{
    static AiConfig inst;
    return inst;
}

QString AiConfig::configFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    std::cout << "Config file path: " << dir.toStdString() << std::endl;
    return dir + "/ai_config.json";
}

void AiConfig::setupDefaults()
{
    m_providers = {
        { "Anthropic", "",
          "https://api.anthropic.com/v1",
          { "claude-opus-4-7",
            "claude-sonnet-4-6",
            "claude-haiku-4-5-20251001" } },
        { "OpenAI", "",
          "https://api.openai.com/v1",
          { "gpt-4o", "gpt-4o-mini", "o1", "o3-mini" } },
        { "Groq", "",
          "https://api.groq.com/openai/v1",
          { "llama-3.3-70b-versatile",
            "llama-3.1-8b-instant",
            "deepseek-r1-distill-llama-70b",
            "gemma2-9b-it",
            "mixtral-8x7b-32768" } },
        { "OpenRouter", "",
          "https://openrouter.ai/api/v1",
          { "anthropic/claude-opus-4-7",
            "anthropic/claude-sonnet-4-6",
            "openai/gpt-4o",
            "openai/gpt-4o-mini",
            "google/gemini-2.0-flash-001",
            "meta-llama/llama-3.3-70b-instruct",
            "deepseek/deepseek-r1",
            "mistralai/mixtral-8x7b-instruct" } },
        { "Gemini", "",
          "https://generativelanguage.googleapis.com/v1beta",
          { "gemini-2.0-flash",
            "gemini-2.0-flash-lite",
            "gemini-2.5-pro-preview-05-06",
            "gemini-1.5-pro",
            "gemini-1.5-flash" } },
        { "Ollama", "",
          "http://localhost:11434/v1",
          { "llama3.3", "llama3.2", "mistral",
            "gemma3", "phi4", "qwen2.5", "deepseek-r1" } }
    };
    m_webSearch = { "", "https://api.search.brave.com/res/v1/web/search", 5 };
}

void AiConfig::load()
{
    setupDefaults();

    const QString path = configFilePath();
    QFile f(path);
    if (!f.exists()) { save(); return; }   // write template on first run
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();
    f.close();

    try {
        const json j = json::parse(raw.constData(), raw.constData() + raw.size());

        if (j.contains("providers")) {
            for (const auto& [key, cfg] : j["providers"].items()) {
                const QString name = QString::fromStdString(key);
                for (auto& p : m_providers) {
                    if (p.name != name) continue;
                    if (cfg.contains("api_key"))
                        p.apiKey  = QString::fromStdString(cfg["api_key"].get<std::string>());
                    if (cfg.contains("base_url"))
                        p.baseUrl = QString::fromStdString(cfg["base_url"].get<std::string>());
                    if (cfg.contains("models")) {
                        p.models.clear();
                        for (const auto& m : cfg["models"])
                            p.models.append(QString::fromStdString(m.get<std::string>()));
                    }
                    break;
                }
            }
        }

        if (j.contains("web_search")) {
            const auto& ws = j["web_search"];
            if (ws.contains("api_key"))
                m_webSearch.apiKey = QString::fromStdString(ws["api_key"].get<std::string>());
            if (ws.contains("base_url"))
                m_webSearch.baseUrl = QString::fromStdString(ws["base_url"].get<std::string>());
            if (ws.contains("max_results"))
                m_webSearch.maxResults = ws["max_results"].get<int>();
        }
    } catch (...) {}
}

void AiConfig::save() const
{
    json j;
    for (const auto& p : m_providers) {
        json pj;
        pj["api_key"]  = p.apiKey.toStdString();
        pj["base_url"] = p.baseUrl.toStdString();
        json arr = json::array();
        for (const auto& m : p.models) arr.push_back(m.toStdString());
        pj["models"] = arr;
        j["providers"][p.name.toStdString()] = pj;
    }
    j["web_search"]["api_key"]     = m_webSearch.apiKey.toStdString();
    j["web_search"]["base_url"]    = m_webSearch.baseUrl.toStdString();
    j["web_search"]["max_results"] = m_webSearch.maxResults;

    QFile f(configFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const std::string s = j.dump(2);
        f.write(s.c_str(), static_cast<qint64>(s.size()));
    }
}

QStringList AiConfig::providerNames() const
{
    QStringList out;
    for (const auto& p : m_providers) out.append(p.name);
    return out;
}

const AiProviderConfig* AiConfig::provider(const QString& name) const
{
    for (const auto& p : m_providers) if (p.name == name) return &p;
    return nullptr;
}

AiProviderConfig* AiConfig::provider(const QString& name)
{
    for (auto& p : m_providers) if (p.name == name) return &p;
    return nullptr;
}
