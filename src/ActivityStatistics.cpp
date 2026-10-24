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

#include "ActivityStatistics.h"

#include "Database.h"

#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVariantMap>

namespace {

/* Everything anew, in this order: all activities together as type '', then
 * per type. Land cover rows left behind by a deleted activity are left out
 * by the join. */
const char *const kUpdate[] = {
    "DELETE FROM statistics",
    "DELETE FROM statistics_land_cover",
    "INSERT INTO statistics (type, activities, duration, distance) "
        "SELECT '', COUNT(*), TOTAL(duration), TOTAL(distance) FROM activities",
    "INSERT INTO statistics (type, activities, duration, distance) "
        "SELECT type, COUNT(*), TOTAL(duration), TOTAL(distance) FROM activities GROUP BY type",
    "INSERT INTO statistics_land_cover (type, kind, distance) "
        "SELECT '', l.kind, TOTAL(l.distance) FROM activity_land_cover l "
        "JOIN activities a ON a.id = l.activity WHERE l.kind <> '' GROUP BY l.kind",
    "INSERT INTO statistics_land_cover (type, kind, distance) "
        "SELECT a.type, l.kind, TOTAL(l.distance) FROM activity_land_cover l "
        "JOIN activities a ON a.id = l.activity WHERE l.kind <> '' GROUP BY a.type, l.kind",
    /* The land not known: what the activities' rows say, and the whole
     * distance of those whose land cover is still being read off their
     * files, so that the kinds always add up to the distance. */
    "INSERT INTO statistics_land_cover (type, kind, distance) "
        "SELECT s.type, '', "
        "(SELECT TOTAL(l.distance) FROM activity_land_cover l JOIN activities a ON a.id = l.activity "
            "WHERE l.kind = '' AND (s.type = '' OR a.type = s.type)) + "
        "(SELECT TOTAL(a.distance) FROM activities a WHERE (s.type = '' OR a.type = s.type) "
            "AND a.id NOT IN (SELECT activity FROM activity_land_cover)) "
        "FROM statistics s",
};
const int kUpdateCount = int(sizeof(kUpdate) / sizeof(kUpdate[0]));

} // namespace

ActivityStatistics::ActivityStatistics(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

QVariantList ActivityStatistics::read() const
{
    QVariantList statistics;
    if (!m_database->isOpen())
        return statistics;

    QSqlQuery query(m_database->connection());
    QHash<QString, QVariantList> landCover;
    if (!query.exec(QStringLiteral("SELECT type, kind, distance FROM statistics_land_cover "
                                   "WHERE distance > 0 ORDER BY type, kind = '', distance DESC"))) {
        qWarning() << "Cannot read the statistics' land cover" << query.lastError().text();
        return statistics;
    }
    while (query.next()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("key"), query.value(1).toString());
        entry.insert(QStringLiteral("distance"), query.value(2).toDouble());
        landCover[query.value(0).toString()].append(entry);
    }

    if (!query.exec(QStringLiteral("SELECT type, activities, duration, distance FROM statistics "
                                   "ORDER BY type <> '', type"))) {
        qWarning() << "Cannot read the statistics" << query.lastError().text();
        return statistics;
    }
    while (query.next()) {
        const QString type = query.value(0).toString();
        QVariantMap entry;
        entry.insert(QStringLiteral("type"), type);
        entry.insert(QStringLiteral("activities"), query.value(1).toInt());
        entry.insert(QStringLiteral("duration"), query.value(2).toDouble());
        entry.insert(QStringLiteral("distance"), query.value(3).toDouble());
        entry.insert(QStringLiteral("landCover"), landCover.value(type));
        statistics.append(entry);
    }
    return statistics;
}

/* In one transaction: the page never reads half of them, and the phone
 * writes the file once. */
void ActivityStatistics::update()
{
    if (!m_database->isOpen())
        return;
    QSqlDatabase connection = m_database->connection();
    connection.transaction();
    QSqlQuery query(connection);
    for (int i = 0; i < kUpdateCount; ++i) {
        if (!query.exec(QString::fromLatin1(kUpdate[i]))) {
            qWarning() << "Cannot work out the statistics" << query.lastError().text();
            connection.rollback();
            return;
        }
    }
    if (!connection.commit()) {
        qWarning() << "Cannot store the statistics" << connection.lastError().text();
        connection.rollback();
        return;
    }
    emit updated();
}
