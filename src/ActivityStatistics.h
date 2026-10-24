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

#ifndef ACTIVITYSTATISTICS_H
#define ACTIVITYSTATISTICS_H

#include <QObject>
#include <QVariantList>

class Database;

/**
 * The statistics page's totals: how many activities, how long and how far,
 * and how far over each land cover - for all activities together and for
 * each type.
 *
 * Worked out anew from ActivityHistory's tables, activities and
 * activity_land_cover, each time an activity is stored, deleted or its land
 * cover changes (update(), from ActivityHistory::activitiesChanged()), and
 * kept in tables of their own, statistics and statistics_land_cover, which
 * this class owns: the page only reads them. Worked out whole rather than
 * added to, so that they never drift from what is stored.
 */
class ActivityStatistics : public QObject
{
    Q_OBJECT

public:
    explicit ActivityStatistics(Database *database, QObject *parent = nullptr);

    /* What is stored: all activities together first, as type "", then each
     * type that has any. Each a map of type, activities, duration (seconds),
     * distance (metres) and landCover - the keys and metres of
     * Activity.landCover, the most first and the land not known last. Empty
     * without a database. */
    Q_INVOKABLE QVariantList read() const;

public slots:
    /* Works them out and stores them. */
    void update();

signals:
    void updated();

private:
    Database *m_database;
};

#endif // ACTIVITYSTATISTICS_H
