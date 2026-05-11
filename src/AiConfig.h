#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct AiProviderConfig {
    QString     name;
    QString     apiKey;
    QString     baseUrl;
    QStringList models;
};

struct AiWebSearchConfig {
    QString apiKey;
    QString baseUrl    = "https://api.search.brave.com/res/v1/web/search";
    int     maxResults = 5;
};

class AiConfig {
public:
    static AiConfig& instance();

    void    load();
    void    save() const;
    QString configFilePath() const;

    QStringList              providerNames()               const;
    const AiProviderConfig*  provider(const QString& name) const;
    AiProviderConfig*        provider(const QString& name);
    const AiWebSearchConfig& webSearch()                   const { return m_webSearch; }

private:
    AiConfig() = default;
    void setupDefaults();

    QVector<AiProviderConfig> m_providers;
    AiWebSearchConfig         m_webSearch;
};
