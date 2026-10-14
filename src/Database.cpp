/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Chronograph.

    Chronograph is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Chronograph is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Chronograph. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Database.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

namespace {

/* The schema, one statement per version: a file at user_version N has had the
 * first N applied. Append only - files on phones are already past whatever is
 * here, so a statement is never edited, reordered or removed. A new feature
 * adds its CREATE TABLE (or ALTER TABLE) at the end. */
const char *const kSchema[] = {
    /* 1: the steps of each calendar day, keyed by the local date as yyyy-MM-dd
     * so that text order is date order. */
    "CREATE TABLE daily_steps ("
        "day TEXT PRIMARY KEY NOT NULL, "
        "steps INTEGER NOT NULL)",
};
const int kSchemaVersion = int(sizeof(kSchema) / sizeof(kSchema[0]));

const char kFileName[] = "chronograph.db";

} // namespace

Database::Database(const QString &filePath)
    // One connection per file, so that tests can open several side by side.
    : m_connectionName(QStringLiteral("chronograph:") + filePath)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(filePath);

    if (!m_database.open()) {
        qWarning() << "Cannot open database" << filePath << m_database.lastError().text();
        return;
    }
    if (!migrate())
        m_database.close();
}

Database::~Database()
{
    m_database.close();
    // removeDatabase() wants the last handle on the connection gone first.
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::isOpen() const
{
    return m_database.isOpen();
}

QSqlDatabase Database::connection() const
{
    return m_database;
}

QString Database::defaultFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QString::fromLatin1(kFileName));
}

/* Applies whatever the file has not seen yet, all in one transaction, so that a
 * migration interrupted halfway leaves the file as it was. SQLite's DDL and
 * user_version are both transactional. */
bool Database::migrate()
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        qWarning() << "Cannot read the database's schema version" << query.lastError().text();
        return false;
    }
    const int version = query.value(0).toInt();
    query.finish();

    if (version > kSchemaVersion) {
        // Written by a newer Chronograph. The tables this one knows are there.
        qWarning() << "Database schema" << version << "is newer than" << kSchemaVersion;
        return true;
    }
    if (version == kSchemaVersion)
        return true;

    if (!m_database.transaction()) {
        qWarning() << "Cannot migrate the database" << m_database.lastError().text();
        return false;
    }
    for (int i = version; i < kSchemaVersion; ++i) {
        if (!query.exec(QString::fromLatin1(kSchema[i]))) {
            qWarning() << "Database migration" << i + 1 << "failed" << query.lastError().text();
            m_database.rollback();
            return false;
        }
    }
    // A pragma takes no bound values.
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion))
            || !m_database.commit()) {
        qWarning() << "Cannot migrate the database" << m_database.lastError().text();
        m_database.rollback();
        return false;
    }
    qInfo() << "Database schema migrated from" << version << "to" << kSchemaVersion;
    return true;
}
