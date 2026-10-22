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
 * again and their files rewritten - and names the activity being recorded. Every track here is generated.
 *
 * Like tst_stephistory this needs Qt with QtSql and its SQLite driver, so it
 * runs in the build engine against the i486 target, from that target's build
 * directory after one sfdk build there:
 *
 *   cd ../build-harbour-chronograph-4.6.0.13-i486
 *   sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell sh -c '
 *       S=../harbour-chronograph B=../build-tst_activityhistory && mkdir -p $B &&
 *       moc $S/src/ActivityHistory.h -o $B/moc_ActivityHistory.cpp &&
 *       g++ -std=c++11 -Wall -Wextra -fPIC -I$S/src \
 *           $(pkg-config --cflags Qt5Sql | sed "s/-I/-isystem /g") \
 *           $S/tests/tst_activityhistory.cpp $S/src/ActivityHistory.cpp \
 *           $S/src/Database.cpp $S/src/Gpx.cpp $S/src/LandCover.cpp $S/src/Track.cpp \
 *           $B/moc_ActivityHistory.cpp $(pkg-config --libs Qt5Sql) \
 *           -o $B/tst_activityhistory &&
 *       $B/tst_activityhistory'
 */

#include "ActivityHistory.h"
#include "Database.h"
#include "Gpx.h"
#include "LandCover.h"
#include "Track.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

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
    expectEqual("and is migrated to schema version 3", scalar(database, QStringLiteral("PRAGMA user_version")), 3);
    expectEqual("its steps are kept", scalar(database, QStringLiteral("SELECT steps FROM daily_steps")), 5000);
    expectEqual("and it has an empty activities table",
                scalar(database, QStringLiteral("SELECT COUNT(*) FROM activities")), 0);
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
    testPaging(directory.path());
    testNoDatabase(directory.path());
    testRecording(directory.path());

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
