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

#include "Achievements.h"

#include "AchievementCatalogue.h"
#include "Database.h"
#include "LandCover.h"

#include <QDate>
#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVariantMap>

#include <vector>

namespace {

/* What is kept of a category, and the value now. */
struct Row {
    AchievementCatalogue::Award award;
    double current = 0;
};

/* As stored, yyyy-MM-dd, for a Julian day number. */
QString dayText(int day)
{
    return QDate::fromJulianDay(day).toString(QStringLiteral("yyyy-MM-dd"));
}

QHash<QString, Row> readRows(const Database *database)
{
    QHash<QString, Row> rows;
    QSqlQuery query(database->connection());
    if (!query.exec(QStringLiteral("SELECT category, level, reached, best, current FROM achievements"))) {
        qWarning() << "Cannot read the achievements" << query.lastError().text();
        return rows;
    }
    while (query.next()) {
        Row row;
        row.award.level = query.value(1).toInt();
        if (row.award.level > 0)
            row.award.day = int(QDate::fromString(query.value(2).toString(), QStringLiteral("yyyy-MM-dd"))
                                .toJulianDay());
        row.award.best = query.value(3).toDouble();
        row.current = query.value(4).toDouble();
        rows.insert(query.value(0).toString(), row);
    }
    return rows;
}

/* One entry of read(): the catalogue's side of a category and what is kept. */
QVariantMap entryOf(const AchievementCatalogue::Category &category, const Row &row)
{
    QVariantList thresholds;
    for (double threshold : category.thresholds)
        thresholds.append(threshold);
    QVariantMap entry;
    entry.insert(QStringLiteral("key"), QString::fromLatin1(category.key));
    entry.insert(QStringLiteral("group"), QString::fromLatin1(category.group));
    entry.insert(QStringLiteral("unit"), QString::fromLatin1(category.unit));
    entry.insert(QStringLiteral("icon"), QString::fromLatin1(category.icon));
    entry.insert(QStringLiteral("badge"), QString::fromLatin1(category.badge));
    entry.insert(QStringLiteral("minimumDistance"), category.minimumDistance);
    entry.insert(QStringLiteral("thresholds"), thresholds);
    entry.insert(QStringLiteral("level"), row.award.level);
    entry.insert(QStringLiteral("reached"), row.award.level > 0 ? dayText(row.award.day) : QString());
    entry.insert(QStringLiteral("best"), row.award.best);
    entry.insert(QStringLiteral("current"), row.current);
    return entry;
}

} // namespace

Achievements::Achievements(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

QVariantList Achievements::read() const
{
    const QHash<QString, Row> rows = m_database->isOpen() ? readRows(m_database) : QHash<QString, Row>();
    QVariantList achievements;
    for (std::size_t i = 0; i < AchievementCatalogue::categoryCount(); ++i) {
        const AchievementCatalogue::Category &category = AchievementCatalogue::category(i);
        achievements.append(entryOf(category, rows.value(QString::fromLatin1(category.key))));
    }
    return achievements;
}

/* Everything read whole, as for the statistics: never drifting from what is
 * stored is worth more than adding up as activities come. The days are
 * local, as the steps' are: an activity counts for the day and the week it
 * started on where it was recorded. */
void Achievements::update()
{
    if (!m_database->isOpen())
        return;
    QSqlQuery query(m_database->connection());
    // Not cached row by row, which a single pass does not need.
    query.setForwardOnly(true);

    QHash<qint64, std::vector<double>> landCover;
    if (!query.exec(QStringLiteral("SELECT activity, kind, distance FROM activity_land_cover WHERE kind <> ''"))) {
        qWarning() << "Cannot read the activities' land cover" << query.lastError().text();
        return;
    }
    while (query.next()) {
        std::vector<double> &distances = landCover[query.value(0).toLongLong()];
        distances.resize(LandCover::KindCount, 0.0);
        const LandCover::Kind kind = LandCover::fromKey(query.value(1).toString().toStdString());
        distances[std::size_t(kind)] += query.value(2).toDouble();
    }

    /* The days as Julian day numbers, which SQLite works out far faster than
     * QDateTime and QDate do - from a start in UTC, the local day. */
    std::vector<AchievementCatalogue::ActivityRecord> activities;
    if (!query.exec(QStringLiteral("SELECT id, type, CAST(julianday(start, 'localtime') + 0.5 AS INTEGER), "
                                   "duration, distance, climb FROM activities ORDER BY start, id"))) {
        qWarning() << "Cannot read the activities" << query.lastError().text();
        return;
    }
    while (query.next()) {
        AchievementCatalogue::ActivityRecord activity;
        activity.type = query.value(1).toString().toStdString();
        activity.day = query.value(2).toInt();
        activity.duration = query.value(3).toDouble();
        activity.distance = query.value(4).toDouble();
        // NULL, not known yet, reads as 0.
        activity.climb = query.value(5).toDouble();
        activity.landCover = landCover.value(query.value(0).toLongLong());
        activities.push_back(activity);
    }

    std::vector<AchievementCatalogue::DayRecord> days;
    if (!query.exec(QStringLiteral("SELECT CAST(julianday(day) + 0.5 AS INTEGER), steps FROM daily_steps "
                                   "ORDER BY day"))) {
        qWarning() << "Cannot read the steps" << query.lastError().text();
        return;
    }
    while (query.next()) {
        AchievementCatalogue::DayRecord day;
        day.day = query.value(0).toInt();
        day.steps = query.value(1).toDouble();
        days.push_back(day);
    }

    const std::vector<AchievementCatalogue::Progress> progress = AchievementCatalogue::evaluate(activities, days);
    const QHash<QString, Row> rows = readRows(m_database);

    /* Only the rows that changed, in one transaction - mostly none or the
     * steps' alone, as this comes with every save of them. */
    QSqlDatabase connection = m_database->connection();
    bool writing = false;
    QVariantList reached;
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO achievements (category, level, reached, best, current) "
                                 "VALUES (:category, :level, :reached, :best, :current)"));
    for (std::size_t i = 0; i < progress.size(); ++i) {
        const AchievementCatalogue::Category &category = AchievementCatalogue::category(i);
        const QString key = QString::fromLatin1(category.key);
        const bool known = rows.contains(key);
        const Row kept = rows.value(key);
        const AchievementCatalogue::Award award = AchievementCatalogue::merge(kept.award, progress[i]);
        if (known && award.level == kept.award.level && award.day == kept.award.day
                && award.best == kept.award.best && progress[i].value == kept.current)
            continue;
        if (!writing) {
            connection.transaction();
            writing = true;
        }
        query.bindValue(QStringLiteral(":category"), key);
        query.bindValue(QStringLiteral(":level"), award.level);
        query.bindValue(QStringLiteral(":reached"), award.level > 0 ? dayText(award.day) : QStringLiteral(""));
        query.bindValue(QStringLiteral(":best"), award.best);
        query.bindValue(QStringLiteral(":current"), progress[i].value);
        if (!query.exec()) {
            qWarning() << "Cannot store the achievement" << key << query.lastError().text();
            connection.rollback();
            return;
        }
        if (award.level > kept.award.level) {
            Row now;
            now.award = award;
            now.current = progress[i].value;
            reached.append(entryOf(category, now));
        }
    }
    if (!writing)
        return;
    if (!connection.commit()) {
        qWarning() << "Cannot store the achievements" << connection.lastError().text();
        connection.rollback();
        return;
    }
    emit updated();
    if (!reached.isEmpty())
        emit awarded(reached);
}
