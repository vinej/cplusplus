#pragma once

#include <QDate>
#include <QMap>
#include <QString>
#include <QVector>

struct PortfolioPosition {
    int     id             = 0;
    QString symbol;
    QString name;
    double  quantity       = 0.0;
    double  cost           = 0.0;   // cost per share
    QDate   dateAcquired;
    double  originalWeight = 0.0;   // stored percentage, e.g. 25.0 = 25%
};

class PortfolioDb {
public:
    static PortfolioDb& instance();
    bool open();

    QVector<QPair<int,QString>> portfolios() const;
    int  createPortfolio(const QString& name);
    bool deletePortfolio(int id);

    QVector<PortfolioPosition> positions(int portfolioId) const;
    bool savePositions(int portfolioId, const QVector<PortfolioPosition>& positions);
    bool updateOriginalWeights(int portfolioId, const QMap<QString,double>& weights);

    QString lastError() const;

private:
    PortfolioDb() = default;
    bool createSchema();
    mutable QString m_lastError;
};
