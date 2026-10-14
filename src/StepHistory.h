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

#ifndef STEPHISTORY_H
#define STEPHISTORY_H

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <functional>

class Database;

/**
 * The steps of each calendar day, kept in the daily_steps table.
 *
 * Takes the steps StepCounter finds and adds them to today's count, which it
 * starts from whatever is stored for today - so a restart carries on where the
 * last run stopped. At midnight the count starts over for the new day.
 *
 * Today's count is not written with every step, which while walking would be
 * twice a second. It is saved once the steps have paused for a while, and at
 * the latest every few minutes during a long walk; at the end of a day; and
 * when this object goes, which is when the app is closed. So a crash or a kill
 * loses at most the last few minutes.
 */
class StepHistory : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int todaySteps READ todaySteps NOTIFY todayStepsChanged)
    /* The days before today, newest first, as maps of "day" (yyyy-MM-dd) and
     * "steps" - 0 for a day without any. */
    Q_PROPERTY(QVariantList recentDays READ recentDays NOTIFY recentDaysChanged)

public:
    /* Where "now" comes from; tests set the time themselves. */
    typedef std::function<QDateTime()> Clock;

    explicit StepHistory(Database *database, Clock clock = &QDateTime::currentDateTime,
                         QObject *parent = nullptr);
    ~StepHistory() override;

    int todaySteps() const;
    QVariantList recentDays() const;

public slots:
    void addSteps(int count);
    /* Writes today's count if it has changed since the last save. */
    void save();
    /* Starts a new day if the date has moved on, or back, since the last look. */
    void checkDate();

signals:
    void todayStepsChanged();
    void recentDaysChanged();

private:
    void startDay(const QDate &day);
    int loadSteps(const QDate &day) const;
    void loadRecentDays();
    void scheduleMidnight();

    Database *m_database;
    Clock m_clock;
    QDate m_day;
    int m_todaySteps = 0;
    bool m_unsaved = false;
    QVariantList m_recentDays;
    /* Restarted by every step: runs out once walking pauses. */
    QTimer m_pauseTimer;
    /* Started by the first unsaved step: bounds what a long walk leaves unsaved. */
    QTimer m_latestTimer;
    QTimer m_midnightTimer;
};

#endif // STEPHISTORY_H
