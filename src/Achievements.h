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

#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <QObject>
#include <QVariantList>

class Database;

/**
 * The achievements page's levels: for each of AchievementCatalogue's
 * categories, the level reached, when, the best value ever and the value
 * now.
 *
 * Worked out anew from the stored activities and the days' steps - the
 * tables of ActivityHistory and StepHistory, which it only reads - each time
 * an activity is stored, deleted or its land cover changes, as the steps
 * are saved, and once as the app starts (update()).
 * Kept in a table of its own, achievements, which this class owns, and
 * merged into what is there: a level once reached is never taken away.
 */
class Achievements : public QObject
{
    Q_OBJECT

public:
    explicit Achievements(Database *database, QObject *parent = nullptr);

    /* Every category, in the catalogue's order, each a map of key, group,
     * unit, icon, badge, minimumDistance and thresholds (one per level) from
     * the catalogue, and level (0 for none), reached (the local date as
     * yyyy-MM-dd, empty for none), best and current - in the category's
     * unit. Every level 0 without a database. */
    Q_INVOKABLE QVariantList read() const;

public slots:
    /* Works them out and stores what changed. */
    void update();

signals:
    void updated();
    /* Levels reached by an update, each an entry as read() has it with the
     * new level, in the catalogue's order. Comes after updated(), within the
     * call that caused it - for a stored activity, within
     * ActivityHistory::add(), before the activity is Finished. */
    void awarded(const QVariantList &entries);

private:
    Database *m_database;
};

#endif // ACHIEVEMENTS_H
