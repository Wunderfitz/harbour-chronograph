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
#include "Track.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

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

} // namespace

ActivityHistory::ActivityHistory(Database *database, const QString &directory, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
    , m_directory(directory)
{
    fetchMore(QModelIndex());
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
        QSqlQuery query(m_database->connection());
        query.prepare(QStringLiteral(
                "INSERT INTO activities (type, start, duration, distance, file) "
                "VALUES (:type, :start, :duration, :distance, :file)"));
        query.bindValue(QStringLiteral(":type"), entry.type);
        query.bindValue(QStringLiteral(":start"), startText(entry.start));
        query.bindValue(QStringLiteral(":duration"), entry.duration);
        query.bindValue(QStringLiteral(":distance"), entry.distance);
        query.bindValue(QStringLiteral(":file"), entry.fileName);
        if (query.exec())
            entry.id = query.lastInsertId().toLongLong();
        else
            qWarning() << "Cannot store the activity" << entry.fileName << query.lastError().text();
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
    return entry.fileName;
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
