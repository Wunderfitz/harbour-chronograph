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

#include "StepHistory.h"

#include "Database.h"

#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

namespace {

/* Saved once no step has come for this long - the end of a walk, or a stop on
 * the way - so that a walk is usually written once, after it. */
const int kSavePauseMs = 30 * 1000;

/* ...and during a long walk at least this often: what a crash or a kill can
 * lose at most. */
const int kSaveLatestMs = 5 * 60 * 1000;

/* How many days before today recentDays holds. */
const int kRecentDays = 3;

/* The midnight timer aims this far past midnight, so that it does not wake up
 * a moment early and find the day unchanged. */
const int kMidnightSlackMs = 1000;

QString dayKey(const QDate &day)
{
    return day.toString(Qt::ISODate);
}

} // namespace

StepHistory::StepHistory(Database *database, Clock clock, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_clock(clock)
{
    m_pauseTimer.setSingleShot(true);
    m_pauseTimer.setInterval(kSavePauseMs);
    connect(&m_pauseTimer, &QTimer::timeout, this, &StepHistory::save);

    m_latestTimer.setSingleShot(true);
    m_latestTimer.setInterval(kSaveLatestMs);
    connect(&m_latestTimer, &QTimer::timeout, this, &StepHistory::save);

    /* Precise, because a coarse timer may be off by 5 % - over a day, more than
     * an hour. The phone does not sleep while counting, so it fires on time. */
    m_midnightTimer.setSingleShot(true);
    m_midnightTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_midnightTimer, &QTimer::timeout, this, [this]() {
        checkDate();
        // Re-armed even if the day has not changed, should the timer be early.
        scheduleMidnight();
    });

    startDay(m_clock().date());
}

/* The last save, when the app is closed: main() destroys this object once the
 * event loop has ended. */
StepHistory::~StepHistory()
{
    save();
}

int StepHistory::todaySteps() const
{
    return m_todaySteps;
}

QVariantList StepHistory::recentDays() const
{
    return m_recentDays;
}

/* Steps count for the day they are counted on. A walk across midnight is split
 * where it is confirmed, which may put a few steps from before midnight into
 * the new day. */
void StepHistory::addSteps(int count)
{
    if (count <= 0)
        return;
    checkDate();

    m_todaySteps += count;
    emit todayStepsChanged();

    m_unsaved = true;
    m_pauseTimer.start();
    if (!m_latestTimer.isActive())
        m_latestTimer.start();
}

void StepHistory::save()
{
    m_pauseTimer.stop();
    m_latestTimer.stop();
    if (!m_unsaved || !m_database->isOpen())
        return;

    /* The day is the key and the count the only value, so replacing the row is
     * all an update needs. */
    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO daily_steps (day, steps) VALUES (:day, :steps)"));
    query.bindValue(QStringLiteral(":day"), dayKey(m_day));
    query.bindValue(QStringLiteral(":steps"), m_todaySteps);
    if (!query.exec()) {
        // Left unsaved: the next step tries again.
        qWarning() << "Cannot save the steps of" << dayKey(m_day) << query.lastError().text();
        return;
    }
    m_unsaved = false;
}

void StepHistory::checkDate()
{
    const QDate today = m_clock().date();
    if (today == m_day)
        return;
    save();
    startDay(today);
}

/* Normally the new day has nothing stored yet. It does when the app is restarted
 * the same day, or when the clock has been set back to a day it has seen. */
void StepHistory::startDay(const QDate &day)
{
    m_day = day;
    m_todaySteps = loadSteps(day);
    emit todayStepsChanged();
    loadRecentDays();
    scheduleMidnight();
}

int StepHistory::loadSteps(const QDate &day) const
{
    if (!m_database->isOpen())
        return 0;

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT steps FROM daily_steps WHERE day = :day"));
    query.bindValue(QStringLiteral(":day"), dayKey(day));
    if (!query.exec()) {
        qWarning() << "Cannot load the steps of" << dayKey(day) << query.lastError().text();
        return 0;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

void StepHistory::loadRecentDays()
{
    QHash<QString, int> stored;
    if (m_database->isOpen()) {
        QSqlQuery query(m_database->connection());
        query.prepare(QStringLiteral(
                "SELECT day, steps FROM daily_steps WHERE day >= :first AND day < :today"));
        query.bindValue(QStringLiteral(":first"), dayKey(m_day.addDays(-kRecentDays)));
        query.bindValue(QStringLiteral(":today"), dayKey(m_day));
        if (query.exec()) {
            while (query.next())
                stored.insert(query.value(0).toString(), query.value(1).toInt());
        } else {
            qWarning() << "Cannot load the recent days" << query.lastError().text();
        }
    }

    m_recentDays.clear();
    for (int daysAgo = 1; daysAgo <= kRecentDays; ++daysAgo) {
        const QString day = dayKey(m_day.addDays(-daysAgo));
        QVariantMap entry;
        entry.insert(QStringLiteral("day"), day);
        entry.insert(QStringLiteral("steps"), stored.value(day, 0));
        m_recentDays.append(entry);
    }
    emit recentDaysChanged();
}

void StepHistory::scheduleMidnight()
{
    const QDateTime now = m_clock();
    QDateTime midnight(now.date().addDays(1), QTime(0, 0));
    /* Where daylight saving time starts at midnight, the day starts at 1:00;
     * aiming at a time that does not exist would wake the timer every second. */
    if (!midnight.isValid())
        midnight = QDateTime(now.date().addDays(1), QTime(1, 0));
    // At most a day and an hour, should the clock change: well within an int.
    m_midnightTimer.start(int(qMax(qint64(0), now.msecsTo(midnight))) + kMidnightSlackMs);
}
