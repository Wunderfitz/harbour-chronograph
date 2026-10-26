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

#include "ActivityHistory.h"

#include "Database.h"
#include "Gpx.h"
#include "LandCover.h"
#include "Track.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include <algorithm>

namespace {

/* How many activities are read at a time: the first page is what the list
 * starts with, the next ones come as it is scrolled. */
const int kPageSize = 100;

const char kDirectoryName[] = "gpx";

/* The start as stored: UTC, to the second. */
QString startText(const QDateTime &start)
{
    return start.toUTC().toString(Qt::ISODate);
}

/* Metres over each land cover, the whole track. */
std::vector<double> distancesOf(const Track &track)
{
    return track.distanceByLandCover(int(track.size()) - 1, LandCover::KindCount);
}

/* Metres climbed, the whole track. */
double climbOf(const Track &track)
{
    return track.stateAt(track.duration()).elevationGain;
}

} // namespace

ActivityHistory::ActivityHistory(Database *database, const QString &directory, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
    , m_directory(directory)
{
    connect(&m_summaryReader, &QThread::finished, this, &ActivityHistory::handleMissingSummaries);
    fetchMore(QModelIndex());
    readMissingSummaries();
}

/* Files still being read are left for the next start. */
ActivityHistory::~ActivityHistory()
{
    disconnect(&m_summaryReader, nullptr, this, nullptr);
}

int ActivityHistory::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant ActivityHistory::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return QVariant();
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TypeRole:
        return entry.type;
    case StartRole:
        return entry.start.toLocalTime();
    case DurationRole:
        return entry.duration;
    case DistanceRole:
        return entry.distance;
    case FileNameRole:
        return entry.fileName;
    }
    return QVariant();
}

QHash<int, QByteArray> ActivityHistory::roleNames() const
{
    QHash<int, QByteArray> names;
    names.insert(TypeRole, "type");
    names.insert(StartRole, "start");
    names.insert(DurationRole, "duration");
    names.insert(DistanceRole, "distance");
    names.insert(FileNameRole, "fileName");
    return names;
}

bool ActivityHistory::canFetchMore(const QModelIndex &parent) const
{
    return !parent.isValid() && !m_complete;
}

/* The next page: the activities that come after the oldest one listed, by start
 * and then by id. Keyed rather than counted, so an activity added at the top in
 * the meantime does not shift the page. */
void ActivityHistory::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid() || m_complete)
        return;
    if (!m_database->isOpen()) {
        m_complete = true;
        return;
    }

    QSqlQuery query(m_database->connection());
    QString sql = QStringLiteral("SELECT id, type, start, duration, distance, file FROM activities ");
    if (!m_entries.isEmpty())
        sql += QStringLiteral("WHERE start < :start OR (start = :start AND id < :id) ");
    sql += QStringLiteral("ORDER BY start DESC, id DESC LIMIT %1").arg(kPageSize);
    query.prepare(sql);
    if (!m_entries.isEmpty()) {
        query.bindValue(QStringLiteral(":start"), startText(m_entries.last().start));
        query.bindValue(QStringLiteral(":id"), m_entries.last().id);
    }
    if (!query.exec()) {
        qWarning() << "Cannot load the activities" << query.lastError().text();
        m_complete = true;
        return;
    }

    QList<Entry> page;
    while (query.next()) {
        Entry entry;
        entry.id = query.value(0).toLongLong();
        entry.type = query.value(1).toString();
        entry.start = QDateTime::fromString(query.value(2).toString(), Qt::ISODate).toUTC();
        entry.duration = query.value(3).toDouble();
        entry.distance = query.value(4).toDouble();
        entry.fileName = query.value(5).toString();
        page.append(entry);
    }
    if (page.size() < kPageSize)
        m_complete = true;
    if (page.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size() + page.size() - 1);
    m_entries.append(page);
    endInsertRows();
}

QString ActivityHistory::directory() const
{
    return m_directory;
}

QString ActivityHistory::filePath(const QString &fileName) const
{
    return QDir(m_directory).filePath(fileName);
}

QString ActivityHistory::add(const QString &type, const Track &track)
{
    if (track.isEmpty())
        return QString();

    Entry entry;
    entry.type = type;
    // To the second, as stored, so that the list sorts the same either way.
    entry.start = QDateTime::fromMSecsSinceEpoch(qint64(track.startTime()) * 1000, Qt::UTC);
    entry.duration = track.duration();
    entry.distance = track.stateAt(entry.duration).distance;
    entry.fileName = newFileName(entry.start);

    QDir().mkpath(m_directory);
    if (!Gpx::write(filePath(entry.fileName), track, type))
        return QString();

    if (m_database->isOpen()) {
        // The row and its land cover at once, and with a single write.
        QSqlDatabase connection = m_database->connection();
        connection.transaction();
        QSqlQuery query(connection);
        query.prepare(QStringLiteral(
                "INSERT INTO activities (type, start, duration, distance, file, climb) "
                "VALUES (:type, :start, :duration, :distance, :file, :climb)"));
        query.bindValue(QStringLiteral(":type"), entry.type);
        query.bindValue(QStringLiteral(":start"), startText(entry.start));
        query.bindValue(QStringLiteral(":duration"), entry.duration);
        query.bindValue(QStringLiteral(":distance"), entry.distance);
        query.bindValue(QStringLiteral(":file"), entry.fileName);
        query.bindValue(QStringLiteral(":climb"), climbOf(track));
        if (query.exec()) {
            entry.id = query.lastInsertId().toLongLong();
            storeLandCover(entry.id, distancesOf(track));
        } else {
            qWarning() << "Cannot store the activity" << entry.fileName << query.lastError().text();
        }
        if (!connection.commit()) {
            qWarning() << "Cannot store the activity" << entry.fileName << connection.lastError().text();
            connection.rollback();
            entry.id = -1;
        }
    }

    /* Normally at the top. Should the clock have been set back, further down -
     * and past the rows read so far it is left to come with a later page. */
    int row = 0;
    while (row < m_entries.size() && m_entries.at(row).start > entry.start)
        ++row;
    if (row < m_entries.size() || m_complete) {
        beginInsertRows(QModelIndex(), row, row);
        m_entries.insert(row, entry);
        endInsertRows();
    }
    emit activitiesChanged();
    return entry.fileName;
}

bool ActivityHistory::remove(const QString &fileName)
{
    // Names come from the table; none of them leads out of the directory.
    if (fileName.isEmpty() || QFileInfo(fileName).fileName() != fileName)
        return false;

    if (m_database->isOpen()) {
        QSqlDatabase connection = m_database->connection();
        connection.transaction();
        QSqlQuery query(connection);
        query.prepare(QStringLiteral("DELETE FROM activity_land_cover "
                                     "WHERE activity IN (SELECT id FROM activities WHERE file = :file)"));
        query.bindValue(QStringLiteral(":file"), fileName);
        bool deleted = query.exec();
        if (deleted) {
            query.prepare(QStringLiteral("DELETE FROM activities WHERE file = :file"));
            query.bindValue(QStringLiteral(":file"), fileName);
            deleted = query.exec();
        }
        if (!deleted || !connection.commit()) {
            qWarning() << "Cannot delete the activity" << fileName << query.lastError().text()
                       << connection.lastError().text();
            connection.rollback();
            return false;
        }
    }

    const QString path = filePath(fileName);
    if (!QFile::remove(path) && QFileInfo::exists(path))
        qWarning() << "Cannot delete the GPX file" << path;

    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).fileName == fileName) {
            beginRemoveRows(QModelIndex(), row, row);
            m_entries.removeAt(row);
            endRemoveRows();
            break;
        }
    }
    emit activitiesChanged();
    return true;
}

bool ActivityHistory::rewrite(const QString &fileName, const QString &type, const Track &track)
{
    if (fileName.isEmpty() || QFileInfo(fileName).fileName() != fileName || track.isEmpty())
        return false;
    const QString path = filePath(fileName);
    if (!QFileInfo::exists(path))
        return false;

    qint64 id = -1;
    if (m_database->isOpen()) {
        QSqlQuery query(m_database->connection());
        query.prepare(QStringLiteral("SELECT id FROM activities WHERE file = :file"));
        query.bindValue(QStringLiteral(":file"), fileName);
        if (!query.exec()) {
            qWarning() << "Cannot look up the activity" << fileName << query.lastError().text();
            return false;
        }
        if (!query.next())
            return false;
        id = query.value(0).toLongLong();
    }
    if (!Gpx::write(path, track, type))
        return false;

    if (id >= 0) {
        QSqlDatabase connection = m_database->connection();
        connection.transaction();
        if (!storeLandCover(id, distancesOf(track)) || !storeClimb(id, climbOf(track))
                || !connection.commit())
            connection.rollback();
    }
    emit activitiesChanged();
    return true;
}

QObject *ActivityHistory::recording() const
{
    return m_recording;
}

void ActivityHistory::setRecording(QObject *activity)
{
    if (m_recording == activity)
        return;
    m_recording = activity;
    emit recordingChanged();
}

QString ActivityHistory::defaultDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QString::fromLatin1(kDirectoryName));
}

/* The start in UTC in ISO 8601's basic format, 20261016T073000Z.gpx: sorts by
 * time, means the same in every time zone and, without colons, is a valid name
 * on any file system the file may be copied to. A second activity started in
 * the same second gets a suffix. */
QString ActivityHistory::newFileName(const QDateTime &start) const
{
    const QString base = start.toUTC().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    QString name = base + QStringLiteral(".gpx");
    for (int n = 2; QFileInfo::exists(filePath(name)); ++n)
        name = QStringLiteral("%1-%2.gpx").arg(base).arg(n);
    return name;
}

/* Replaces the activity's rows. The land not known always has one, if only
 * with 0: any row at all is what says the activity has been summed up. */
bool ActivityHistory::storeLandCover(qint64 id, const std::vector<double> &distances)
{
    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("DELETE FROM activity_land_cover WHERE activity = :activity"));
    query.bindValue(QStringLiteral(":activity"), id);
    bool stored = query.exec();
    query.prepare(QStringLiteral("INSERT INTO activity_land_cover (activity, kind, distance) "
                                 "VALUES (:activity, :kind, :distance)"));
    const int kinds = std::min(int(distances.size()), int(LandCover::KindCount));
    for (int kind = LandCover::Unknown; stored && kind < kinds; ++kind) {
        const double distance = distances[std::size_t(kind)];
        if (kind != LandCover::Unknown && !(distance > 0))
            continue;
        query.bindValue(QStringLiteral(":activity"), id);
        query.bindValue(QStringLiteral(":kind"), QString::fromLatin1(LandCover::key(LandCover::Kind(kind))));
        query.bindValue(QStringLiteral(":distance"), distance);
        stored = query.exec();
    }
    if (!stored)
        qWarning() << "Cannot store the land cover of activity" << id << query.lastError().text();
    return stored;
}

bool ActivityHistory::storeClimb(qint64 id, double climb)
{
    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("UPDATE activities SET climb = :climb WHERE id = :id"));
    query.bindValue(QStringLiteral(":climb"), climb);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        qWarning() << "Cannot store the climb of activity" << id << query.lastError().text();
        return false;
    }
    return true;
}

/* The activities stored before their land cover or their climb went into the
 * database. Read at most once each: the next start finds them summed up. */
void ActivityHistory::readMissingSummaries()
{
    if (!m_database->isOpen())
        return;
    QSqlQuery query(m_database->connection());
    if (!query.exec(QStringLiteral("SELECT id, file FROM activities WHERE climb IS NULL "
                                   "OR id NOT IN (SELECT activity FROM activity_land_cover)"))) {
        qWarning() << "Cannot look up the activities' land cover and climb" << query.lastError().text();
        return;
    }
    std::vector<TrackSummaryReader::Entry> entries;
    while (query.next()) {
        TrackSummaryReader::Entry entry;
        entry.id = query.value(0).toLongLong();
        entry.filePath = filePath(query.value(1).toString());
        entries.push_back(entry);
    }
    if (entries.empty())
        return;
    qInfo() << "Reading the land cover and climb of" << entries.size() << "activities";
    m_summaryReader.setEntries(entries);
    m_summaryReader.start(QThread::LowPriority);
}

/* Into the activities still stored and still without - one replayed
 * meanwhile may have had its land cover rewritten, and its climb with it. A
 * file that cannot be read counts as land not known and no climb, so that it
 * is not read again at every start. */
void ActivityHistory::handleMissingSummaries()
{
    if (!m_database->isOpen())
        return;
    QSqlDatabase connection = m_database->connection();
    connection.transaction();
    QSqlQuery query(connection);
    query.prepare(QStringLiteral("SELECT distance, climb IS NULL, "
                                 "id NOT IN (SELECT activity FROM activity_land_cover) "
                                 "FROM activities WHERE id = :id"));
    for (const TrackSummaryReader::Entry &entry : m_summaryReader.entries()) {
        query.bindValue(QStringLiteral(":id"), entry.id);
        if (!query.exec() || !query.next())
            continue;
        const bool withoutClimb = query.value(1).toBool();
        const bool withoutLandCover = query.value(2).toBool();
        std::vector<double> distances = entry.distances;
        if (!entry.read) {
            distances.assign(LandCover::KindCount, 0.0);
            distances[LandCover::Unknown] = query.value(0).toDouble();
        }
        query.finish();
        if (withoutLandCover)
            storeLandCover(entry.id, distances);
        if (withoutClimb)
            storeClimb(entry.id, entry.read ? entry.climb : 0.0);
    }
    if (!connection.commit()) {
        qWarning() << "Cannot store the activities' land cover and climb" << connection.lastError().text();
        connection.rollback();
        return;
    }
    emit activitiesChanged();
}
