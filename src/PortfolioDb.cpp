#include "PortfolioDb.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

PortfolioDb& PortfolioDb::instance()
{
    static PortfolioDb inst;
    return inst;
}

bool PortfolioDb::open()
{
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection))
        return true; // already open

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dir + "/portfolios.db");

    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return createSchema();
}

bool PortfolioDb::createSchema()
{
    QSqlQuery q;
    q.exec("PRAGMA foreign_keys = ON");

    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS portfolios ("
        "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT UNIQUE NOT NULL"
        ")")) {
        m_lastError = q.lastError().text();
        return false;
    }

    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS positions ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  portfolio_id    INTEGER NOT NULL REFERENCES portfolios(id) ON DELETE CASCADE,"
        "  symbol          TEXT NOT NULL,"
        "  name            TEXT NOT NULL DEFAULT '',"
        "  quantity        REAL NOT NULL DEFAULT 0,"
        "  cost            REAL NOT NULL DEFAULT 0,"
        "  date_acquired   TEXT NOT NULL DEFAULT '',"
        "  original_weight REAL NOT NULL DEFAULT 0"
        ")")) {
        m_lastError = q.lastError().text();
        return false;
    }

    return true;
}

QVector<QPair<int,QString>> PortfolioDb::portfolios() const
{
    QVector<QPair<int,QString>> result;
    QSqlQuery q("SELECT id, name FROM portfolios ORDER BY name");
    while (q.next())
        result.append({q.value(0).toInt(), q.value(1).toString()});
    return result;
}

int PortfolioDb::createPortfolio(const QString& name)
{
    QSqlQuery q;
    q.prepare("INSERT INTO portfolios (name) VALUES (?)");
    q.addBindValue(name);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool PortfolioDb::deletePortfolio(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM portfolios WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

QVector<PortfolioPosition> PortfolioDb::positions(int portfolioId) const
{
    QVector<PortfolioPosition> result;
    QSqlQuery q;
    q.prepare(
        "SELECT id, symbol, name, quantity, cost, date_acquired, original_weight "
        "FROM positions WHERE portfolio_id = ? ORDER BY rowid");
    q.addBindValue(portfolioId);
    if (!q.exec()) { m_lastError = q.lastError().text(); return result; }

    while (q.next()) {
        PortfolioPosition p;
        p.id             = q.value(0).toInt();
        p.symbol         = q.value(1).toString();
        p.name           = q.value(2).toString();
        p.quantity       = q.value(3).toDouble();
        p.cost           = q.value(4).toDouble();
        p.dateAcquired   = QDate::fromString(q.value(5).toString(), Qt::ISODate);
        p.originalWeight = q.value(6).toDouble();
        result.append(p);
    }
    return result;
}

bool PortfolioDb::savePositions(int portfolioId, const QVector<PortfolioPosition>& positions)
{
    QSqlQuery q;
    q.prepare("DELETE FROM positions WHERE portfolio_id = ?");
    q.addBindValue(portfolioId);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }

    for (const auto& pos : positions) {
        q.prepare(
            "INSERT INTO positions "
            "(portfolio_id, symbol, name, quantity, cost, date_acquired, original_weight) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(portfolioId);
        q.addBindValue(pos.symbol);
        q.addBindValue(pos.name);
        q.addBindValue(pos.quantity);
        q.addBindValue(pos.cost);
        q.addBindValue(pos.dateAcquired.toString(Qt::ISODate));
        q.addBindValue(pos.originalWeight);
        if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    }
    return true;
}

bool PortfolioDb::updateOriginalWeights(int portfolioId, const QMap<QString,double>& weights)
{
    QSqlQuery q;
    for (auto it = weights.cbegin(); it != weights.cend(); ++it) {
        q.prepare("UPDATE positions SET original_weight = ? WHERE portfolio_id = ? AND symbol = ?");
        q.addBindValue(it.value());
        q.addBindValue(portfolioId);
        q.addBindValue(it.key());
        if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    }
    return true;
}

QString PortfolioDb::lastError() const { return m_lastError; }
