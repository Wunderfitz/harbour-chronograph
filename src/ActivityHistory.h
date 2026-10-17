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

#ifndef ACTIVITYHISTORY_H
#define ACTIVITYHISTORY_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

class Database;
class Track;

/**
 * The recorded activities, newest first: the list on the main page.
 *
 * Each activity is a row in the activities table, with what the list shows -
 * type, start, duration and distance - and the name of its GPX file, which
 * holds the track itself and lives in a directory of its own. This class owns
 * both: Activity hands it a finished track, and it writes the file and the row.
 *
 * The list is read a page at a time. The first page comes with the
 * constructor; the view asks for the next ones through fetchMore() as it
 * scrolls towards the end of what it has.
 */
class ActivityHistory : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        /* The activity type's key: "running", "walking", "cycling", "rowing" or
         * "driving". */
        TypeRole = Qt::UserRole + 1,
        /* Local time, which reaches QML as a Date. */
        StartRole,
        /* Seconds, pauses left out. */
        DurationRole,
        /* Metres. */
        DistanceRole,
        /* The GPX file, relative to directory(). */
        FileNameRole
    };

    ActivityHistory(Database *database, const QString &directory, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    QString directory() const;
    QString filePath(const QString &fileName) const;

    /* Stores a finished activity: writes its GPX file, named after its start,
     * and its row, and puts it into the list. Returns the file's name, or an
     * empty string if there was nothing to store or the file could not be
     * written. */
    QString add(const QString &type, const Track &track);

    /* gpx/ below the app's data directory. */
    static QString defaultDirectory();

private:
    struct Entry {
        qint64 id = -1;
        QString type;
        QDateTime start;
        double duration = 0;
        double distance = 0;
        QString fileName;
    };

    QString newFileName(const QDateTime &start) const;

    Database *m_database;
    QString m_directory;
    QList<Entry> m_entries;
    /* Every row has been read. */
    bool m_complete = false;
};

#endif // ACTIVITYHISTORY_H
