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

/*
 * Checks for what keeps the activities: the migration that adds their table,
 * GPX files written and read back - pauses, the receiver's speeds and all - and
 * read from other apps, and ActivityHistory's list, which is read a page at a
 * time, newest first, takes new activities at the top and lets them be deleted
 * again and their files rewritten - and names the activity being recorded.
 * Also each activity's distance over each land cover and its climb, stored
 * with it or read off the files of activities from before, ActivityStatistics,
 * which works the totals out from both, and Achievements, which works out the
 * levels from them and the steps and keeps them. Every track here is
 * generated.
 *
 * Like tst_stephistory this needs Qt with QtSql and its SQLite driver, so it
 * runs in the build engine against the i486 target, from that target's build
 * directory after one sfdk build there:
 *
 *   cd ../build-harbour-chronograph-4.6.0.13-i486
 *   sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell sh -c '
 *       S=../harbour-chronograph B=../build-tst_activityhistory && mkdir -p $B &&
 *       for h in ActivityHistory ActivityStatistics Achievements TrackSummaryReader; do
 *           moc $S/src/$h.h -o $B/moc_$h.cpp || exit 1; done &&
 *       g++ -std=c++11 -Wall -Wextra -fPIC -I$S/src \
 *           $(pkg-config --cflags Qt5Sql | sed "s/-I/-isystem /g") \
 *           $S/tests/tst_activityhistory.cpp $S/src/ActivityHistory.cpp \
 *           $S/src/ActivityStatistics.cpp $S/src/Achievements.cpp \
 *           $S/src/AchievementCatalogue.cpp $S/src/TrackSummaryReader.cpp \
 *           $S/src/Database.cpp $S/src/Gpx.cpp $S/src/LandCover.cpp $S/src/Track.cpp \
 *           $B/moc_ActivityHistory.cpp $B/moc_ActivityStatistics.cpp \
 *           $B/moc_Achievements.cpp $B/moc_TrackSummaryReader.cpp \
 *           $(pkg-config --libs Qt5Sql) -o $B/tst_activityhistory &&
 *       $B/tst_activityhistory'
 */

#include "AchievementCatalogue.h"
#include "Achievements.h"
#include "ActivityHistory.h"
#include "ActivityStatistics.h"
#include "Database.h"
#include "Gpx.h"
#include "LandCover.h"
#include "Track.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

/* 2026-10-16 07:30:00 UTC. */
const double kStartTime = 1792135800.0;

void expect(const char *name, bool ok)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        ++g_failures;
}

void expectEqual(const char *name, double actual, double expected, double tolerance = 0)
{
    const bool ok = std::fabs(actual - expected) <= tolerance;
    std::printf("%s %-60s %10.3f (expected %.3f)\n", ok ? "PASS" : "FAIL", name, actual, expected);
    if (!ok)
        ++g_failures;
}

void expectText(const char *name, const QString &actual, const QString &expected)
{
    const bool ok = actual == expected;
    std::printf("%s %-60s %s (expected %s)\n", ok ? "PASS" : "FAIL", name,
                qPrintable(actual), qPrintable(expected));
    if (!ok)
        ++g_failures;
}

int scalar(const Database &database, const QString &sql)
{
    QSqlQuery query(database.connection());
    if (!query.exec(sql) || !query.next())
        return -1;
    return query.value(0).toInt();
}

/* Metres stored for an activity, named by its file, over a land cover by
 * its key; -1 for no row. */
double landCoverOf(const Database &database, const QString &fileName, const QString &kind)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("SELECT l.distance FROM activity_land_cover l "
                                 "JOIN activities a ON a.id = l.activity "
                                 "WHERE a.file = :file AND l.kind = :kind"));
    query.bindValue(QStringLiteral(":file"), fileName);
    // Never a null string, which would be bound as NULL.
    query.bindValue(QStringLiteral(":kind"), kind.isNull() ? QStringLiteral("") : kind);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toDouble();
}

int landCoverRows(const Database &database, const QString &fileName)
{
    return scalar(database, QStringLiteral("SELECT COUNT(*) FROM activity_land_cover l "
                                           "JOIN activities a ON a.id = l.activity "
                                           "WHERE a.file = '%1'").arg(fileName));
}

/* Metres climbed stored for an activity, named by its file; -1 for none,
 * -2 for NULL. */
double climbOf(const Database &database, const QString &fileName)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("SELECT climb FROM activities WHERE file = :file"));
    query.bindValue(QStringLiteral(":file"), fileName);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).isNull() ? -2 : query.value(0).toDouble();
}

/* Runs the event loop until the history says its activities changed, or
 * `timeout` ms have passed; whether it did. */
bool waitForChanges(const ActivityHistory &history, int timeout = 10000)
{
    QEventLoop loop;
    bool came = false;
    QObject::connect(&history, &ActivityHistory::activitiesChanged, &loop, [&loop, &came]() {
        came = true;
        loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    loop.exec();
    return came;
}

/* An activity's row put straight into the table, as one stored before its
 * land cover was. */
void storeOld(const Database &database, const QString &type, double start, double distance,
              const QString &fileName)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("INSERT INTO activities (type, start, duration, distance, file) "
                                 "VALUES (:type, :start, 60, :distance, :file)"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":start"),
                    QDateTime::fromMSecsSinceEpoch(qint64(start) * 1000, Qt::UTC).toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":distance"), distance);
    query.bindValue(QStringLiteral(":file"), fileName);
    query.exec();
}

/* Metres over each land cover, as Track has them. */
std::vector<double> distancesOf(const Track &track)
{
    return track.distanceByLandCover(int(track.size()) - 1, LandCover::KindCount);
}

/* The land cover of points `from` up to `to`, not included. */
void setLandCover(Track *track, std::size_t from, std::size_t to, LandCover::Kind kind)
{
    for (std::size_t i = from; i < to && i < track->size(); ++i)
        track->setLandCover(i, kind, 16);
}

/* One entry of ActivityStatistics::read(), by type; an empty map for none. */
QVariantMap statisticsOf(const QVariantList &statistics, const QString &type)
{
    for (const QVariant &entry : statistics) {
        if (entry.toMap().value(QStringLiteral("type")).toString() == type)
            return entry.toMap();
    }
    return QVariantMap();
}

/* The metres of a land cover in such an entry; -1 for none. */
double landCoverIn(const QVariantMap &statistics, const QString &kind)
{
    for (const QVariant &entry : statistics.value(QStringLiteral("landCover")).toList()) {
        if (entry.toMap().value(QStringLiteral("key")).toString() == kind)
            return entry.toMap().value(QStringLiteral("distance")).toDouble();
    }
    return -1;
}

/* One entry of Achievements::read(), by key; an empty map for none. */
QVariantMap achievementOf(const QVariantList &achievements, const QString &key)
{
    for (const QVariant &entry : achievements) {
        if (entry.toMap().value(QStringLiteral("key")).toString() == key)
            return entry.toMap();
    }
    return QVariantMap();
}

int levelOf(const QVariantList &achievements, const QString &key)
{
    return achievementOf(achievements, key).value(QStringLiteral("level")).toInt();
}

/* A day's steps put straight into their table. */
void storeSteps(const Database &database, const QString &day, int steps)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO daily_steps (day, steps) VALUES (:day, :steps)"));
    query.bindValue(QStringLiteral(":day"), day);
    query.bindValue(QStringLiteral(":steps"), steps);
    query.exec();
}

/* A run north at 3 m/s from `start`, one fix a second, with a pause of
 * `pause` seconds after the first half if it is not 0. */
Track makeTrack(double start, int seconds, double pause = 0)
{
    Track track;
    double time = start;
    track.begin(time);
    for (int i = 0; i < seconds; ++i) {
        if (pause > 0 && i == seconds / 2) {
            track.end(time);
            time += pause;
            track.begin(time);
        }
        time += 1.0;
        TrackPoint point;
        point.time = time;
        point.latitude = 52.5 + 3.0 * i / 111195.08;
        point.longitude = 13.4;
        point.altitude = 40.0 + 0.1 * i;
        point.accuracy = 4.0;
        point.verticalAccuracy = 6.0;
        point.speed = 3.0;
        point.course = 0.5;
        track.addPoint(point);
    }
    track.end(time + 0.5);
    return track;
}

/* An activity's row put straight into the table. */
void store(const Database &database, const QString &start, double distance)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("INSERT INTO activities (type, start, duration, distance, file) "
                                 "VALUES ('walking', :start, 60, :distance, 'none.gpx')"));
    query.bindValue(QStringLiteral(":start"), start);
    query.bindValue(QStringLiteral(":distance"), distance);
    query.exec();
}

double distanceAt(const ActivityHistory &history, int row)
{
    return history.data(history.index(row), ActivityHistory::DistanceRole).toDouble();
}

void testMigration(const QString &directory)
{
    const QString file = QDir(directory).filePath(QStringLiteral("old.db"));
    {
        // A file as 0.2 left it.
        QSqlDatabase old = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("old"));
        old.setDatabaseName(file);
        old.open();
        QSqlQuery query(old);
        query.exec(QStringLiteral("CREATE TABLE daily_steps (day TEXT PRIMARY KEY NOT NULL, steps INTEGER NOT NULL)"));
        query.exec(QStringLiteral("INSERT INTO daily_steps VALUES ('2026-10-14', 5000)"));
        query.exec(QStringLiteral("PRAGMA user_version = 1"));
        query.finish();
        old.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("old"));

    Database database(file);
    expect("a database from 0.2 opens", database.isOpen());
    expectEqual("and is migrated to schema version 9", scalar(database, QStringLiteral("PRAGMA user_version")), 9);
    expectEqual("its steps are kept", scalar(database, QStringLiteral("SELECT steps FROM daily_steps")), 5000);
    expectEqual("and it has an empty activities table",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM activities")), 0);
    expectEqual("an empty table for their land cover",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM activity_land_cover")), 0);
    expectEqual("and empty ones for the statistics",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM statistics"))
                + scalar(database, QStringLiteral("SELECT COUNT(*) FROM statistics_land_cover")), 0);
    expectEqual("and the achievements", scalar(database, QStringLiteral("SELECT COUNT(*) FROM achievements")), 0);
    expectEqual("and their occasions",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM achievement_occasions")), 0);
}

void testGpxRoundTrip(const QString &directory)
{
    Track written = makeTrack(kStartTime, 120, 45);
    written.setLandCover(10, LandCover::Forest, 16);
    written.setLandCover(11, LandCover::Water, 12);
    const QString file = QDir(directory).filePath(QStringLiteral("round.gpx"));
    expect("a GPX file is written", Gpx::write(file, written, QStringLiteral("cycling")));

    Track read;
    QString type;
    expect("and read back", Gpx::read(file, &read, &type));
    expectText("with its type", type, QStringLiteral("cycling"));
    expectEqual("all points", double(read.size()), double(written.size()));
    expectEqual("both segments", double(read.segments().size()), 2);
    expectEqual("the same duration, pause left out", read.duration(), written.duration(), 0.002);
    expectEqual("the same distance", read.stateAt(read.duration()).distance,
                written.stateAt(written.duration()).distance, 0.05);
    expectEqual("the same climb", read.stateAt(read.duration()).elevationGain,
                written.stateAt(written.duration()).elevationGain, 0.2);
    expectEqual("the same start", read.startTime(), kStartTime, 0.0005);
    expectEqual("a point's speed", read.point(10).speed, 3.0, 0.005);
    expectEqual("its course", read.point(10).course, 0.5, 0.05);
    expectEqual("its accuracy", read.point(10).accuracy, 4.0, 0.05);
    expectEqual("its vertical accuracy", read.point(10).verticalAccuracy, 6.0, 0.05);
    expectEqual("its altitude", read.point(10).altitude, 41.0, 0.05);
    expectEqual("its time, to the millisecond", read.point(10).time, written.point(10).time, 0.0005);
    expect("its land cover", read.point(10).landCover == LandCover::Forest
                             && read.point(10).landCoverLevel == 16);
    expect("the next one's", read.point(11).landCover == LandCover::Water
                             && read.point(11).landCoverLevel == 12);
    expect("none where none was known", read.point(12).landCover == LandCover::Unknown
                                        && read.point(12).landCoverLevel == 0);

    QFile in(file);
    in.open(QIODevice::ReadOnly);
    const QString text = QString::fromUtf8(in.readAll());
    expect("the file is GPX 1.1", text.contains(QStringLiteral("xmlns=\"http://www.topografix.com/GPX/1/1\""))
                                  && text.contains(QStringLiteral("version=\"1.1\"")));
    expect("with the speed in Garmin's extension", text.contains(QStringLiteral("<gpxtpx:speed>3.00</gpxtpx:speed>")));
    expect("and the land cover in Chronograph's",
           text.contains(QStringLiteral("<chronograph:landCover level=\"16\">forest</chronograph:landCover>")));
    expectEqual("only where it is known", text.count(QStringLiteral("landCover level")), 2);
}

void testForeignGpx(const QString &directory)
{
    const QString file = QDir(directory).filePath(QStringLiteral("foreign.gpx"));
    QFile out(file);
    out.open(QIODevice::WriteOnly);
    out.write("<?xml version=\"1.0\"?>\n"
              "<gpx version=\"1.0\" xmlns=\"http://www.topografix.com/GPX/1/0\">\n"
              " <trk><name>Morning</name><type>walking</type><trkseg>\n"
              "  <trkpt lat=\"52.5\" lon=\"13.4\"><time>2026-10-16T07:30:01Z</time><speed>1.4</speed></trkpt>\n"
              "  <trkpt lat=\"52.50005\" lon=\"13.4\"><time>2026-10-16T07:30:05Z</time><speed>1.4</speed>"
              "<extensions><landCover level=\"15\">swamp</landCover></extensions></trkpt>\n"
              "  <trkpt lat=\"52.5001\" lon=\"13.4\"></trkpt>\n"
              "  <trkpt lat=\"52.5001\" lon=\"13.4\"><time>2026-10-16T07:30:09.500+00:00</time></trkpt>\n"
              " </trkseg></trk>\n"
              "</gpx>\n");
    out.close();

    Track track;
    QString type;
    expect("a GPX 1.0 file from elsewhere reads", Gpx::read(file, &track, &type));
    expectText("its type, after a name", type, QStringLiteral("walking"));
    expectEqual("a point without a time is left out", double(track.size()), 3);
    expectEqual("the segment runs from its first point to its last", track.duration(), 8.5, 0.001);
    expectEqual("GPX 1.0's own speed is read", track.point(0).speed, 1.4, 0.001);
    expectEqual("the distance", track.stateAt(track.duration()).distance, 11.1, 0.2);
    expect("a land cover of no known kind is left out", track.point(1).landCover == LandCover::Unknown
                                                        && track.point(1).landCoverLevel == 0);

    Track none;
    expect("a missing file does not read", !Gpx::read(QDir(directory).filePath(QStringLiteral("missing.gpx")), &none, &type));
}

void testAdding(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-adding"));
    Database database(QDir(directory).filePath(QStringLiteral("adding.db")));
    ActivityHistory history(&database, gpx);
    expectEqual("a new history is empty", history.rowCount(), 0);
    expect("and has nothing more to fetch", !history.canFetchMore(QModelIndex()));

    expectText("an empty track is not stored", history.add(QStringLiteral("running"), Track()), QString());

    const Track first = makeTrack(kStartTime, 60);
    const QString name = history.add(QStringLiteral("running"), first);
    expectText("the file is named after the start, in UTC", name, QStringLiteral("20261016T073000Z.gpx"));
    expect("and is in the gpx directory", QFileInfo::exists(QDir(gpx).filePath(name)));
    expectEqual("the activity is listed", history.rowCount(), 1);
    expectEqual("its row is stored", scalar(database, QStringLiteral("SELECT COUNT(*) FROM activities")), 1);

    const QModelIndex row = history.index(0);
    expectText("its type", history.data(row, ActivityHistory::TypeRole).toString(), QStringLiteral("running"));
    expectEqual("its duration", history.data(row, ActivityHistory::DurationRole).toDouble(), 60.5, 0.001);
    expectEqual("its distance", history.data(row, ActivityHistory::DistanceRole).toDouble(),
                first.stateAt(first.duration()).distance, 0.001);
    expect("its start, in local time",
           history.data(row, ActivityHistory::StartRole).toDateTime()
           == QDateTime::fromMSecsSinceEpoch(qint64(kStartTime) * 1000).toLocalTime());

    const QString again = history.add(QStringLiteral("walking"), makeTrack(kStartTime + 0.4, 30));
    expectText("one started in the same second gets a suffix", again, QStringLiteral("20261016T073000Z-2.gpx"));

    history.add(QStringLiteral("cycling"), makeTrack(kStartTime + 3600, 30));
    expectText("the newest goes to the top",
               history.data(history.index(0), ActivityHistory::TypeRole).toString(), QStringLiteral("cycling"));

    ActivityHistory reopened(&database, gpx);
    expectEqual("a restart lists them all", reopened.rowCount(), 3);
    expectText("newest first", reopened.data(reopened.index(0), ActivityHistory::TypeRole).toString(),
               QStringLiteral("cycling"));
    expectText("and the same second by the order they were stored",
               reopened.data(reopened.index(1), ActivityHistory::FileNameRole).toString(), again);
}

void testRemoving(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-removing"));
    Database database(QDir(directory).filePath(QStringLiteral("removing.db")));
    ActivityHistory history(&database, gpx);
    const QString older = history.add(QStringLiteral("walking"), makeTrack(kStartTime, 30));
    const QString middle = history.add(QStringLiteral("running"), makeTrack(kStartTime + 3600, 30));
    const QString newer = history.add(QStringLiteral("cycling"), makeTrack(kStartTime + 7200, 30));

    expect("an activity in the middle is deleted", history.remove(middle));
    expectEqual("it leaves the list", history.rowCount(), 2);
    expectText("the newer stays above", history.data(history.index(0), ActivityHistory::FileNameRole).toString(), newer);
    expectText("the older below", history.data(history.index(1), ActivityHistory::FileNameRole).toString(), older);
    expectEqual("its row is gone", scalar(database, QStringLiteral("SELECT COUNT(*) FROM activities")), 2);
    expect("and its file", !QFileInfo::exists(QDir(gpx).filePath(middle)));
    expect("the others' files stay", QFileInfo::exists(QDir(gpx).filePath(older))
                                      && QFileInfo::exists(QDir(gpx).filePath(newer)));

    ActivityHistory reopened(&database, gpx);
    expectEqual("a restart lists only those left", reopened.rowCount(), 2);

    // Its file gone already, the row still goes.
    QFile::remove(QDir(gpx).filePath(older));
    expect("an activity whose file is missing is deleted", history.remove(older));
    expectEqual("and leaves the list", history.rowCount(), 1);

    expect("a name with a path is refused", !history.remove(QStringLiteral("../removing.db")));
    expect("and the file it names stays", QFileInfo::exists(QDir(directory).filePath(QStringLiteral("removing.db"))));
    expect("an empty name is refused", !history.remove(QString()));
    expectEqual("and nothing else is deleted", history.rowCount(), 1);

    // A new one in the same second as a deleted one may take its name again.
    expectText("the name of a deleted activity is free again",
               history.add(QStringLiteral("running"), makeTrack(kStartTime + 3600, 30)), middle);
}

/* As the land cover comes in after the stop. */
void testRewriting(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-rewriting"));
    Database database(QDir(directory).filePath(QStringLiteral("rewriting.db")));
    ActivityHistory history(&database, gpx);
    Track track = makeTrack(kStartTime, 60);
    const QString name = history.add(QStringLiteral("walking"), track);
    for (std::size_t i = 0; i < track.size(); ++i)
        track.setLandCover(i, LandCover::Grassland, 13);

    expect("a stored activity's file is written anew", history.rewrite(name, QStringLiteral("walking"), track));
    Track read;
    QString type;
    Gpx::read(QDir(gpx).filePath(name), &read, &type);
    expect("with the land cover learnt since", read.point(30).landCover == LandCover::Grassland
                                               && read.point(30).landCoverLevel == 13);
    expectText("and its type", type, QStringLiteral("walking"));
    expectEqual("all its points", double(read.size()), double(track.size()));
    expectEqual("its row stays the one", scalar(database, QStringLiteral("SELECT COUNT(*) FROM activities")), 1);
    expectEqual("and so does the list", history.rowCount(), 1);

    expect("a name with a path is refused",
           !history.rewrite(QStringLiteral("../rewriting.db"), QStringLiteral("walking"), track));
    expect("an empty track is not written", !history.rewrite(name, QStringLiteral("walking"), Track()));

    // A file the table does not list, as one left behind by a failed delete.
    QFile stray(QDir(gpx).filePath(QStringLiteral("stray.gpx")));
    stray.open(QIODevice::WriteOnly);
    stray.write("stray");
    stray.close();
    expect("a file the history does not list is not written",
           !history.rewrite(QStringLiteral("stray.gpx"), QStringLiteral("walking"), track));
    expectEqual("and keeps what it held", double(QFileInfo(stray.fileName()).size()), 5);

    history.remove(name);
    expect("a deleted activity is not written", !history.rewrite(name, QStringLiteral("walking"), track));
    expect("nor brought back", !QFileInfo::exists(QDir(gpx).filePath(name)));
}

/* Each activity's distance over each land cover, kept with its row. */
void testLandCoverRows(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-landcover"));
    Database database(QDir(directory).filePath(QStringLiteral("landcover.db")));
    ActivityHistory history(&database, gpx);
    int changes = 0;
    QObject::connect(&history, &ActivityHistory::activitiesChanged, [&changes]() { ++changes; });

    Track track = makeTrack(kStartTime, 60);
    setLandCover(&track, 0, 20, LandCover::Forest);
    setLandCover(&track, 20, 40, LandCover::Water);
    const std::vector<double> distances = distancesOf(track);
    const QString name = history.add(QStringLiteral("walking"), track);
    expect("storing an activity says so", changes == 1);
    expectEqual("its forest is stored with it", landCoverOf(database, name, QStringLiteral("forest")),
                distances[LandCover::Forest], 0.001);
    expectEqual("its water", landCoverOf(database, name, QStringLiteral("water")),
                distances[LandCover::Water], 0.001);
    expectEqual("and the land not known", landCoverOf(database, name, QString()),
                distances[LandCover::Unknown], 0.001);
    expectEqual("nothing for the land it did not cross", landCoverRows(database, name), 3);
    expect("the test crosses all three", distances[LandCover::Forest] > 0 && distances[LandCover::Water] > 0
                                         && distances[LandCover::Unknown] > 0);
    const double climb = track.stateAt(track.duration()).elevationGain;
    expectEqual("its climb is stored with it", climbOf(database, name), climb, 0.001);
    expect("the test climbs", climb > 0);

    setLandCover(&track, 0, track.size(), LandCover::Grassland);
    QSqlQuery(database.connection()).exec(QStringLiteral("UPDATE activities SET climb = NULL"));
    history.rewrite(name, QStringLiteral("walking"), track);
    expect("a rewrite says so", changes == 2);
    expectEqual("and stores the climb again", climbOf(database, name), climb, 0.001);
    expectEqual("and replaces it", landCoverOf(database, name, QStringLiteral("grassland")),
                distancesOf(track)[LandCover::Grassland], 0.001);
    expectEqual("the land not known keeps its row at 0", landCoverOf(database, name, QString()), 0);
    expectEqual("and nothing else is left", landCoverRows(database, name), 2);

    const qint64 rows = scalar(database, QStringLiteral("SELECT COUNT(*) FROM activity_land_cover"));
    history.remove(name);
    expect("deleting says so", changes == 3);
    expectEqual("and takes the land cover along",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM activity_land_cover")), rows - 2);
}

/* Activities stored before their land cover went into the database have it
 * read off their files once. */
void testMissingLandCover(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-missing"));
    Database database(QDir(directory).filePath(QStringLiteral("missing.db")));
    Track track = makeTrack(kStartTime, 60);
    setLandCover(&track, 0, 30, LandCover::Forest);
    const std::vector<double> distances = distancesOf(track);
    QDir().mkpath(gpx);
    Gpx::write(QDir(gpx).filePath(QStringLiteral("old.gpx")), track, QStringLiteral("running"));
    storeOld(database, QStringLiteral("running"), kStartTime, track.stateAt(track.duration()).distance,
             QStringLiteral("old.gpx"));
    storeOld(database, QStringLiteral("walking"), kStartTime + 3600, 123, QStringLiteral("gone.gpx"));
    Gpx::write(QDir(gpx).filePath(QStringLiteral("deleted.gpx")), track, QStringLiteral("running"));
    storeOld(database, QStringLiteral("running"), kStartTime + 7200, 456, QStringLiteral("deleted.gpx"));
    // One that has its land cover but not its climb, as 0.3's last ones.
    Gpx::write(QDir(gpx).filePath(QStringLiteral("summed.gpx")), track, QStringLiteral("running"));
    storeOld(database, QStringLiteral("running"), kStartTime + 9000, 789, QStringLiteral("summed.gpx"));
    QSqlQuery(database.connection()).exec(QStringLiteral(
            "INSERT INTO activity_land_cover SELECT id, 'water', 789 FROM activities WHERE file = 'summed.gpx'"));

    ActivityHistory history(&database, gpx);
    // Before the files are read.
    history.remove(QStringLiteral("deleted.gpx"));
    expect("an old activity's land cover is read off its file in the background",
           waitForChanges(history));
    // The file keeps the positions to a few millimetres.
    expectEqual("its forest", landCoverOf(database, QStringLiteral("old.gpx"), QStringLiteral("forest")),
                distances[LandCover::Forest], 0.01);
    expectEqual("and the land not known", landCoverOf(database, QStringLiteral("old.gpx"), QString()),
                distances[LandCover::Unknown], 0.01);
    expectEqual("one whose file is gone counts as not known",
                landCoverOf(database, QStringLiteral("gone.gpx"), QString()), 123);
    expectEqual("one deleted meanwhile is not brought back",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM activity_land_cover")), 4);
    expectEqual("its climb is read along", climbOf(database, QStringLiteral("old.gpx")),
                track.stateAt(track.duration()).elevationGain, 0.01);
    expectEqual("one whose file is gone climbed nothing", climbOf(database, QStringLiteral("gone.gpx")), 0);
    expectEqual("one with its land cover has its climb read",
                climbOf(database, QStringLiteral("summed.gpx")), track.stateAt(track.duration()).elevationGain,
                0.01);
    expectEqual("and keeps its land cover as it was",
                landCoverOf(database, QStringLiteral("summed.gpx"), QStringLiteral("water")), 789);
    expectEqual("without one of the land not known",
                landCoverOf(database, QStringLiteral("summed.gpx"), QString()), -1);

    ActivityHistory again(&database, gpx);
    expect("a restart reads none again",
           !waitForChanges(again, 500));
}

void testStatistics(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-statistics"));
    Database database(QDir(directory).filePath(QStringLiteral("statistics.db")));
    ActivityHistory history(&database, gpx);
    ActivityStatistics statistics(&database);
    int updates = 0;
    QObject::connect(&history, &ActivityHistory::activitiesChanged, &statistics, &ActivityStatistics::update);
    QObject::connect(&statistics, &ActivityStatistics::updated, [&updates]() { ++updates; });
    expect("none before the first activity", statistics.read().isEmpty());

    Track forest = makeTrack(kStartTime, 60);
    setLandCover(&forest, 0, 30, LandCover::Forest);
    Track unknown = makeTrack(kStartTime + 3600, 90, 30);
    Track water = makeTrack(kStartTime + 7200, 40);
    setLandCover(&water, 0, water.size(), LandCover::Water);
    history.add(QStringLiteral("running"), forest);
    const QString unknownName = history.add(QStringLiteral("running"), unknown);
    const QString waterName = history.add(QStringLiteral("walking"), water);
    expectEqual("worked out with each activity stored", updates, 3);

    const double forestDistance = forest.stateAt(forest.duration()).distance;
    const double unknownDistance = unknown.stateAt(unknown.duration()).distance;
    const double waterDistance = water.stateAt(water.duration()).distance;
    QVariantList read = statistics.read();
    expectEqual("all activities and each type that has any", read.size(), 3);
    expectText("all activities first", read.value(0).toMap().value(QStringLiteral("type")).toString(), QString());
    QVariantMap all = statisticsOf(read, QString());
    expectEqual("all: how many", all.value(QStringLiteral("activities")).toInt(), 3);
    expectEqual("all: how long", all.value(QStringLiteral("duration")).toDouble(),
                forest.duration() + unknown.duration() + water.duration(), 0.01);
    expectEqual("all: how far", all.value(QStringLiteral("distance")).toDouble(),
                forestDistance + unknownDistance + waterDistance, 0.01);
    expectEqual("all: forest", landCoverIn(all, QStringLiteral("forest")),
                distancesOf(forest)[LandCover::Forest], 0.01);
    expectEqual("all: water", landCoverIn(all, QStringLiteral("water")), distancesOf(water)[LandCover::Water], 0.01);
    expectEqual("all: the land not known", landCoverIn(all, QString()),
                distancesOf(forest)[LandCover::Unknown] + unknownDistance, 0.01);
    const QVariantList kinds = all.value(QStringLiteral("landCover")).toList();
    expectText("the land not known last", kinds.value(kinds.size() - 1).toMap().value(QStringLiteral("key")).toString(),
               QString());
    expect("the others the most first",
           kinds.value(0).toMap().value(QStringLiteral("distance")).toDouble()
           >= kinds.value(1).toMap().value(QStringLiteral("distance")).toDouble());

    QVariantMap running = statisticsOf(read, QStringLiteral("running"));
    expectEqual("running: how many", running.value(QStringLiteral("activities")).toInt(), 2);
    expectEqual("running: how long, pauses left out", running.value(QStringLiteral("duration")).toDouble(),
                forest.duration() + unknown.duration(), 0.01);
    expectEqual("running: no water", landCoverIn(running, QStringLiteral("water")), -1);
    QVariantMap walking = statisticsOf(read, QStringLiteral("walking"));
    expectEqual("walking: how far", walking.value(QStringLiteral("distance")).toDouble(), waterDistance, 0.01);
    expectEqual("walking: all of it water", landCoverIn(walking, QStringLiteral("water")), waterDistance, 0.01);
    expectEqual("walking: no land not known where there is none", landCoverIn(walking, QString()), -1);
    expect("no type without activities", statisticsOf(read, QStringLiteral("cycling")).isEmpty());

    setLandCover(&unknown, 0, unknown.size(), LandCover::BuiltUp);
    history.rewrite(unknownName, QStringLiteral("running"), unknown);
    expectEqual("worked out anew as the land cover is learnt", updates, 4);
    running = statisticsOf(statistics.read(), QStringLiteral("running"));
    expectEqual("running: built-up now", landCoverIn(running, QStringLiteral("builtup")), unknownDistance, 0.01);
    expectEqual("running: only the forest run's land not known",
                landCoverIn(running, QString()), distancesOf(forest)[LandCover::Unknown], 0.01);

    history.remove(waterName);
    expectEqual("worked out anew as one is deleted", updates, 5);
    read = statistics.read();
    expect("walking is gone", statisticsOf(read, QStringLiteral("walking")).isEmpty());
    all = statisticsOf(read, QString());
    expectEqual("all: one less", all.value(QStringLiteral("activities")).toInt(), 2);
    expectEqual("all: no water", landCoverIn(all, QStringLiteral("water")), -1);

    // One still waiting for its file to be read.
    storeOld(database, QStringLiteral("cycling"), kStartTime + 10800, 500, QStringLiteral("waiting.gpx"));
    statistics.update();
    read = statistics.read();
    expectEqual("one whose land cover is not read yet counts as not known",
                landCoverIn(statisticsOf(read, QStringLiteral("cycling")), QString()), 500, 0.01);
    expectEqual("in all of them too", landCoverIn(statisticsOf(read, QString()), QString()),
                distancesOf(forest)[LandCover::Unknown] + 500, 0.01);

    ActivityStatistics reopened(&database);
    expect("a restart reads them as stored, without working them out", reopened.read() == read);
}

/* What working them out costs, with more activities than anyone records. */
void testStatisticsCost(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("cost.db")));
    const char *const types[] = { "running", "walking", "cycling", "rowing", "driving" };
    const char *const kinds[] = { "forest", "farmland", "builtup", "" };
    database.connection().transaction();
    QSqlQuery query(database.connection());
    for (int i = 0; i < 2000; ++i) {
        storeOld(database, QString::fromLatin1(types[i % 5]), kStartTime + i * 3600.0, 4000,
                 QStringLiteral("cost.gpx"));
        for (const char *kind : kinds)
            query.exec(QStringLiteral("INSERT INTO activity_land_cover VALUES (%1, '%2', 1000)")
                       .arg(i + 1).arg(QString::fromLatin1(kind)));
    }
    database.connection().commit();

    ActivityStatistics statistics(&database);
    QElapsedTimer timer;
    timer.start();
    statistics.update();
    const qint64 updated = timer.elapsed();
    timer.restart();
    const QVariantList read = statistics.read();
    std::printf("INFO 2000 activities: worked out in %lld ms, read in %lld ms\n",
                updated, qint64(timer.elapsed()));
    expectEqual("2000 activities worked out", statisticsOf(read, QString()).value(QStringLiteral("activities")).toInt(),
                2000);
    expectEqual("their forest", landCoverIn(statisticsOf(read, QString()), QStringLiteral("forest")), 2e6, 0.01);
}

void testAchievements(const QString &directory)
{
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-achievements"));
    Database database(QDir(directory).filePath(QStringLiteral("achievements.db")));
    ActivityHistory history(&database, gpx);
    Achievements achievements(&database);
    int updates = 0;
    QObject::connect(&history, &ActivityHistory::activitiesChanged, &achievements, &Achievements::update);
    QObject::connect(&achievements, &Achievements::updated, [&updates]() { ++updates; });
    QVariantList awarded;
    int awards = 0;
    QObject::connect(&achievements, &Achievements::awarded, [&awarded, &awards](const QVariantList &entries) {
        awarded = entries;
        ++awards;
    });

    QVariantList read = achievements.read();
    expectEqual("every category before anything is stored", read.size(),
                double(AchievementCatalogue::categoryCount()));
    expectText("in the catalogue's order", read.value(0).toMap().value(QStringLiteral("key")).toString(),
               QString::fromLatin1(AchievementCatalogue::category(0).key));
    expectEqual("with its thresholds", read.value(0).toMap().value(QStringLiteral("thresholds")).toList().size(),
                AchievementCatalogue::kLevels);
    expectEqual("and no level", levelOf(read, QStringLiteral("running-longest")), 0);

    // 1.2 km, 400 s.
    Track run = makeTrack(kStartTime, 400);
    const double distance = run.stateAt(run.duration()).distance;
    const QString name = history.add(QStringLiteral("running"), run);
    expectEqual("worked out with the activity stored", updates, 1);
    expectEqual("the levels it reached are announced, within the storing", awards, 1);
    expectEqual("with the new level", levelOf(awarded, QStringLiteral("running-longest")), 1);
    expectEqual("the first activity among them", levelOf(awarded, QStringLiteral("activities-count")), 1);
    expect("and none it did not reach", achievementOf(awarded, QStringLiteral("walking-longest")).isEmpty()
                                        && achievementOf(awarded, QStringLiteral("streak-weeks")).isEmpty());
    expectEqual("with its value", achievementOf(awarded, QStringLiteral("running-longest"))
                .value(QStringLiteral("best")).toDouble(), distance, 0.01);
    read = achievements.read();
    QVariantMap longest = achievementOf(read, QStringLiteral("running-longest"));
    expectEqual("the first run reaches the first level", longest.value(QStringLiteral("level")).toInt(), 1);
    expectEqual("with its distance as the best", longest.value(QStringLiteral("best")).toDouble(), distance, 0.01);
    expectEqual("and as the value now", longest.value(QStringLiteral("current")).toDouble(), distance, 0.01);
    expectText("reached on the day it was run", longest.value(QStringLiteral("reached")).toString(),
               QDateTime::fromMSecsSinceEpoch(qint64(kStartTime) * 1000, Qt::UTC).toLocalTime().date()
               .toString(QStringLiteral("yyyy-MM-dd")));
    expectEqual("the first activity", levelOf(read, QStringLiteral("activities-count")), 1);
    expectEqual("not a walk", levelOf(read, QStringLiteral("walking-longest")), 0);
    QVariantMap pace = achievementOf(read, QStringLiteral("running-pace"));
    expect("nor the fastest, too short to count", pace.value(QStringLiteral("level")).toInt() == 0
                                                  && pace.value(QStringLiteral("current")).toDouble() == 0);
    expectText("nothing reached, no date", pace.value(QStringLiteral("reached")).toString(), QString());

    // Run on the day of the first operation under anaesthesia.
    const QString runDay = QDateTime::fromMSecsSinceEpoch(qint64(kStartTime) * 1000, Qt::UTC).toLocalTime().date()
                           .toString(QStringLiteral("yyyy-MM-dd"));
    const QVariantMap firstOne = achievementOf(awarded, QStringLiteral("days-first-one"));
    expectEqual("a calendar category is announced too", firstOne.value(QStringLiteral("level")).toInt(), 1);
    const QVariantList newly = firstOne.value(QStringLiteral("newlyCollected")).toList();
    expect("with the occasion that reached it", newly.size() == 1
           && newly.value(0).toMap().value(QStringLiteral("key")).toString() == QStringLiteral("first-anaesthesia")
           && newly.value(0).toMap().value(QStringLiteral("year")).toInt() == 1846
           && newly.value(0).toMap().value(QStringLiteral("reached")).toString() == runDay);
    QVariantMap collected = achievementOf(read, QStringLiteral("days-first-one"));
    expectEqual("and lists it as collected", collected.value(QStringLiteral("collected")).toList().size(), 1);
    expectEqual("of all there is", collected.value(QStringLiteral("total")).toInt(), 42);
    expect("the others list nothing", !longest.contains(QStringLiteral("collected"))
           && achievementOf(read, QStringLiteral("days-patriot")).value(QStringLiteral("collected")).toList().isEmpty());
    expectEqual("stored as an occasion", scalar(database, QStringLiteral("SELECT COUNT(*) FROM achievement_occasions")),
                1);

    achievements.update();
    expectEqual("worked out again with nothing new, nothing is written", updates, 1);
    expectEqual("nor announced", awards, 1);

    storeSteps(database, QStringLiteral("2026-10-15"), 3000);
    storeSteps(database, QStringLiteral("2026-10-16"), 12000);
    achievements.update();
    expectEqual("the steps are worked out too", updates, 2);
    expect("and their levels announced", awards == 2 && levelOf(awarded, QStringLiteral("steps-day")) == 4
                                         && levelOf(awarded, QStringLiteral("steps-total")) == 1
                                         && awarded.size() == 2);
    read = achievements.read();
    expectEqual("the best day's", levelOf(read, QStringLiteral("steps-day")), 4);
    expectText("dated by that day", achievementOf(read, QStringLiteral("steps-day")).value(QStringLiteral("reached"))
               .toString(), QStringLiteral("2026-10-16"));
    expectEqual("and all days'", achievementOf(read, QStringLiteral("steps-total")).value(QStringLiteral("current"))
                .toDouble(), 15000);

    history.remove(name);
    expectEqual("worked out again as the activity is deleted", updates, 3);
    expectEqual("which announces nothing", awards, 2);
    read = achievements.read();
    longest = achievementOf(read, QStringLiteral("running-longest"));
    expectEqual("its level stays", longest.value(QStringLiteral("level")).toInt(), 1);
    expectEqual("and the best", longest.value(QStringLiteral("best")).toDouble(), distance, 0.01);
    expectEqual("though nothing is run now", longest.value(QStringLiteral("current")).toDouble(), 0);
    expectEqual("the activity still counted", levelOf(read, QStringLiteral("activities-count")), 1);
    collected = achievementOf(read, QStringLiteral("days-first-one"));
    expect("and its occasion is still collected", collected.value(QStringLiteral("level")).toInt() == 1
           && collected.value(QStringLiteral("collected")).toList().size() == 1
           && collected.value(QStringLiteral("current")).toInt() == 1);

    history.add(QStringLiteral("driving"), makeTrack(kStartTime + 86400, 2000));
    read = achievements.read();
    expectEqual("a drive counts for nothing", achievementOf(read, QStringLiteral("activities-count"))
                .value(QStringLiteral("current")).toInt(), 0);

    Achievements reopened(&database);
    expect("a restart reads them as stored", reopened.read() == read);

    const QString blocked = QDir(directory).filePath(QStringLiteral("achievements-blocked.db"));
    QDir().mkpath(blocked);
    Database none(blocked);
    Achievements without(&none);
    without.update();
    read = without.read();
    expect("without a database every category is there, at no level",
           read.size() == int(AchievementCatalogue::categoryCount())
           && levelOf(read, QStringLiteral("running-longest")) == 0);
}

/* What working them out costs, with more activities than anyone records and
 * three years of steps. */
void testAchievementsCost(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("achievements-cost.db")));
    const char *const types[] = { "running", "walking", "cycling", "rowing", "driving" };
    const char *const kinds[] = { "forest", "farmland", "builtup", "" };
    database.connection().transaction();
    QSqlQuery query(database.connection());
    for (int i = 0; i < 2000; ++i) {
        storeOld(database, QString::fromLatin1(types[i % 5]), kStartTime + i * 43200.0, 4000,
                 QStringLiteral("cost.gpx"));
        for (const char *kind : kinds)
            query.exec(QStringLiteral("INSERT INTO activity_land_cover VALUES (%1, '%2', 1000)")
                       .arg(i + 1).arg(QString::fromLatin1(kind)));
    }
    query.exec(QStringLiteral("UPDATE activities SET climb = 20"));
    const QDate first(2023, 10, 1);
    for (int day = 0; day < 1100; ++day)
        storeSteps(database, first.addDays(day).toString(QStringLiteral("yyyy-MM-dd")), 8000);
    database.connection().commit();

    Achievements achievements(&database);
    QElapsedTimer timer;
    timer.start();
    achievements.update();
    const qint64 first_ = timer.elapsed();
    storeSteps(database, first.addDays(1099).toString(QStringLiteral("yyyy-MM-dd")), 9000);
    timer.restart();
    achievements.update();
    const qint64 again = timer.elapsed();
    timer.restart();
    const QVariantList read = achievements.read();
    std::printf("INFO 2000 activities, 1100 days: worked out in %lld ms, again after a day's steps in %lld ms, "
                "read in %lld ms\n", first_, again, qint64(timer.elapsed()));
    expectEqual("1600 activities counted, drives aside",
                achievementOf(read, QStringLiteral("activities-count")).value(QStringLiteral("current")).toInt(),
                1600);
    expectEqual("their forest", achievementOf(read, QStringLiteral("terrain-forest"))
                .value(QStringLiteral("current")).toDouble(), 1.6e6, 0.01);
    expectEqual("weeks in a row, every one of them",
                achievementOf(read, QStringLiteral("streak-weeks")).value(QStringLiteral("current")).toInt(),
                std::floor(2000 / 14.0), 2);
}

void testPaging(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("paging.db")));
    // 250 activities a day apart, distance = the day, and two more on the
    // same second as day 150.
    const QDateTime first(QDate(2025, 1, 1), QTime(8, 0), Qt::UTC);
    database.connection().transaction();
    for (int day = 0; day < 250; ++day)
        store(database, first.addDays(day).toString(Qt::ISODate), day);
    store(database, first.addDays(150).toString(Qt::ISODate), 1150);
    store(database, first.addDays(150).toString(Qt::ISODate), 2150);
    database.connection().commit();

    ActivityHistory history(&database, QDir(directory).filePath(QStringLiteral("gpx-paging")));
    expectEqual("the first page holds 100", history.rowCount(), 100);
    expectEqual("the newest first", distanceAt(history, 0), 249);
    expect("there is more to fetch", history.canFetchMore(QModelIndex()));

    history.fetchMore(QModelIndex());
    expectEqual("the second page adds 100", history.rowCount(), 200);
    // Days 249 to 151 and the first of the three on day 150 make the first
    // page, so the page ends among them.
    expectEqual("same second: the last stored first", distanceAt(history, 99), 2150);
    expectEqual("the next page carries on with the one before", distanceAt(history, 100), 1150);
    expectEqual("then the first", distanceAt(history, 101), 150);
    expectEqual("then the day before", distanceAt(history, 102), 149);

    history.add(QStringLiteral("running"), makeTrack(kStartTime, 10));
    expectEqual("an activity added meanwhile goes to the top", history.rowCount(), 201);

    history.fetchMore(QModelIndex());
    expectEqual("the last page brings the rest, none twice", history.rowCount(), 253);
    expectEqual("down to the oldest", distanceAt(history, 252), 0);
    expect("and then there is nothing more", !history.canFetchMore(QModelIndex()));
}

void testNoDatabase(const QString &directory)
{
    const QString file = QDir(directory).filePath(QStringLiteral("blocked.db"));
    QDir().mkpath(file);
    Database database(file);
    const QString gpx = QDir(directory).filePath(QStringLiteral("gpx-blocked"));
    ActivityHistory history(&database, gpx);
    const QString name = history.add(QStringLiteral("running"), makeTrack(kStartTime, 10));
    expect("without a database the GPX file is still written", QFileInfo::exists(QDir(gpx).filePath(name)));
    expectEqual("and the activity listed while the app runs", history.rowCount(), 1);
    expect("and its file rewritten", history.rewrite(name, QStringLiteral("running"), makeTrack(kStartTime, 10)));
    expect("and deleted again", history.remove(name));
    expect("file and all", history.rowCount() == 0 && !QFileInfo::exists(QDir(gpx).filePath(name)));
    expect("and then not rewritten", !history.rewrite(name, QStringLiteral("running"), makeTrack(kStartTime, 10))
                                     && !QFileInfo::exists(QDir(gpx).filePath(name)));
}

/* The activity being recorded, as the cover asks for it: any QObject will do
 * here, as the history knows nothing more of it. */
void testRecording(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("recording.db")));
    ActivityHistory history(&database, QDir(directory).filePath(QStringLiteral("gpx-recording")));
    int changes = 0;
    QObject::connect(&history, &ActivityHistory::recordingChanged, [&changes]() { ++changes; });
    expect("nothing recorded at first", history.recording() == nullptr);

    QObject *activity = new QObject;
    history.setRecording(activity);
    expect("an activity recording is named", history.recording() == activity && changes == 1);
    history.setRecording(activity);
    expectEqual("named again, it changes nothing", changes, 1);
    history.setRecording(nullptr);
    expect("cleared at its stop", history.recording() == nullptr && changes == 2);

    history.setRecording(activity);
    delete activity;
    expect("never left pointing at an activity gone", history.recording() == nullptr);
}

} // namespace

int main(int argc, char *argv[])
{
    // QtSql finds its SQLite driver through the application's plugin paths.
    QCoreApplication app(argc, argv);

    QTemporaryDir directory;
    if (!directory.isValid()) {
        std::printf("FAIL cannot create a temporary directory\n");
        return 1;
    }

    testMigration(directory.path());
    testGpxRoundTrip(directory.path());
    testForeignGpx(directory.path());
    testAdding(directory.path());
    testRemoving(directory.path());
    testRewriting(directory.path());
    testLandCoverRows(directory.path());
    testMissingLandCover(directory.path());
    testStatistics(directory.path());
    testStatisticsCost(directory.path());
    testAchievements(directory.path());
    testAchievementsCost(directory.path());
    testPaging(directory.path());
    testNoDatabase(directory.path());
    testRecording(directory.path());

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
