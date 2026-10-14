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

#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>

/**
 * The app's SQLite database: one file, one connection, and the schema.
 *
 * The schema grows by migrations. A feature that needs a table appends the
 * statements that create it to the list in Database.cpp, and opening the
 * database brings an older file up to date; SQLite's user_version records how
 * far a file has got. The classes that use a table own the queries on it - this
 * one only hands out the connection.
 *
 * A database that cannot be opened or migrated is left closed. The app still
 * counts then, it just keeps nothing.
 */
class Database
{
public:
    explicit Database(const QString &filePath);
    ~Database();

    bool isOpen() const;
    QSqlDatabase connection() const;

    /* chronograph.db below the app's data directory, the one place Sailjail
     * lets the app write to. */
    static QString defaultFilePath();

private:
    bool migrate();

    QString m_connectionName;
    QSqlDatabase m_database;

    Q_DISABLE_COPY(Database)
};

#endif // DATABASE_H
