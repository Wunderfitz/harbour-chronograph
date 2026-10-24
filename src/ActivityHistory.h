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

#include "DistanceByLandCoverReader.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QString>

#include <vector>

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
 *
 * remove() deletes an activity again, row and file; the pages ask for it only
 * once their remorse timer has run out. rewrite() writes a stored activity's
 * file anew, with what has been learnt about it since: its land cover.
 *
 * Each activity's distance over each land cover goes into a table of its
 * own as well, with the row and again with every rewrite, so that the
 * statistics can be worked out without reading a single file. Activities
 * stored before that table was there have it read off their files once, in
 * a thread, as the history is made. activitiesChanged() says when any of
 * this changed.
 *
 * It also knows the activity being recorded, if any, for the cover: the
 * activity page's Activity is QML's, and the cover cannot see into the page.
 */
class ActivityHistory : public QAbstractListModel
{
    Q_OBJECT

    /* The Activity recording or paused now, null while none is. A QObject, so
     * that the history does not depend on what records. */
    Q_PROPERTY(QObject *recording READ recording NOTIFY recordingChanged)

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
    ~ActivityHistory() override;

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

    /* Deletes a stored activity, named by its GPX file: its row, the file,
     * and its place in the list. The row goes first, so that should the file
     * not go, what is left is a file nothing lists rather than an activity
     * that cannot be replayed. Returns false, changing nothing, if the row
     * could not be deleted or the name is not a file of this directory. */
    Q_INVOKABLE bool remove(const QString &fileName);

    /* Writes a stored activity's GPX file anew from its track as it is now;
     * its row and its place in the list stay as they are. Only for an
     * activity still stored, row and file, so that one deleted meanwhile is
     * not brought back: returns false, writing nothing, otherwise. */
    bool rewrite(const QString &fileName, const QString &type, const Track &track);

    QObject *recording() const;
    /* Set by the Activity as it starts recording, cleared as it stops - also
     * when it goes, as it stops then. Null clears it. */
    void setRecording(QObject *activity);

    /* gpx/ below the app's data directory. */
    static QString defaultDirectory();

signals:
    void recordingChanged();
    /* An activity stored, deleted, or its land cover changed - what the
     * statistics are made of. */
    void activitiesChanged();

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
    /* Inside a transaction of the caller's. */
    bool storeLandCover(qint64 id, const std::vector<double> &distances);
    void readMissingLandCover();
    void handleMissingLandCover();

    Database *m_database;
    QString m_directory;
    QList<Entry> m_entries;
    /* Every row has been read. */
    bool m_complete = false;
    QPointer<QObject> m_recording;
    DistanceByLandCoverReader m_landCoverReader;
};

#endif // ACTIVITYHISTORY_H
