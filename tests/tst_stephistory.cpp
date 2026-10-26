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
 * Checks for Database and StepHistory: the schema and its migration, saving
 * only when asked to and when closed, carrying on after a restart, the change of
 * day and the days before today. The clock is the test's own, so midnight comes
 * when a test says so.
 *
 * Unlike tst_stepdetector this needs Qt with QtSql and its SQLite driver, which
 * the host may not have; the build engine has them for the i486 target. Build
 * and run from that target's build directory, after one sfdk build there
 * (build-shell only works at the top of a build tree):
 *
 *   cd ../build-harbour-chronograph-4.6.0.13-i486
 *   sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell sh -c '
 *       S=../harbour-chronograph B=../build-tst_stephistory && mkdir -p $B &&
 *       moc $S/src/StepHistory.h -o $B/moc_StepHistory.cpp &&
 *       g++ -std=c++11 -Wall -Wextra -fPIC -I$S/src \
 *           $(pkg-config --cflags Qt5Sql | sed "s/-I/-isystem /g") \
 *           $S/tests/tst_stephistory.cpp $S/src/Database.cpp $S/src/StepHistory.cpp \
 *           $B/moc_StepHistory.cpp $(pkg-config --libs Qt5Sql) -o $B/tst_stephistory &&
 *       $B/tst_stephistory'
 *
 * Qt 5.6's headers go in as system headers: the engine's compiler is newer
 * than they are and would warn about them under -Wextra.
 */

#include "Database.h"
#include "StepHistory.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QVariantMap>

#include <cstdio>

namespace {

int g_failures = 0;

void expect(const char *name, bool ok)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        ++g_failures;
}

void expectEqual(const char *name, int actual, int expected)
{
    const bool ok = actual == expected;
    std::printf("%s %-56s %6d (expected %d)\n", ok ? "PASS" : "FAIL", name, actual, expected);
    if (!ok)
        ++g_failures;
}

/* A clock the test moves by hand. */
struct TestClock {
    QDateTime now = QDateTime(QDate(2026, 3, 10), QTime(12, 0));
    StepHistory::Clock clock()
    {
        return [this]() { return now; };
    }
};

/* What is stored for `day`, read past StepHistory; -1 for no row. */
int storedSteps(const Database &database, const QString &day)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("SELECT steps FROM daily_steps WHERE day = :day"));
    query.bindValue(QStringLiteral(":day"), day);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

int schemaVersion(const Database &database)
{
    QSqlQuery query(database.connection());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return -1;
    return query.value(0).toInt();
}

void store(const Database &database, const QString &day, int steps)
{
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral("INSERT INTO daily_steps (day, steps) VALUES (:day, :steps)"));
    query.bindValue(QStringLiteral(":day"), day);
    query.bindValue(QStringLiteral(":steps"), steps);
    query.exec();
}

QString recentDay(const StepHistory &history, int i)
{
    return history.recentDays().value(i).toMap().value(QStringLiteral("day")).toString();
}

int recentSteps(const StepHistory &history, int i)
{
    return history.recentDays().value(i).toMap().value(QStringLiteral("steps")).toInt();
}

void testFreshDatabase(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("fresh.db")));
    expect("a new database opens", database.isOpen());
    expectEqual("a new database is at schema version 8", schemaVersion(database), 8);

    TestClock time;
    StepHistory history(&database, time.clock());
    expectEqual("a new database starts the day at 0", history.todaySteps(), 0);
    expectEqual("there are three previous days", history.recentDays().size(), 3);
    expect("the previous days are yesterday first",
           recentDay(history, 0) == QLatin1String("2026-03-09")
           && recentDay(history, 2) == QLatin1String("2026-03-07"));
    expectEqual("a previous day without steps reads 0", recentSteps(history, 1), 0);
}

void testSaving(const QString &directory)
{
    const QString file = QDir(directory).filePath(QStringLiteral("saving.db"));
    TestClock time;
    {
        Database database(file);
        StepHistory history(&database, time.clock());
        history.addSteps(6);
        history.addSteps(2);
        expectEqual("steps add up", history.todaySteps(), 8);
        expectEqual("steps are not written as they come", storedSteps(database, "2026-03-10"), -1);
        history.save();
        expectEqual("save() writes today's count", storedSteps(database, "2026-03-10"), 8);
        history.addSteps(0);
        history.addSteps(-3);
        expectEqual("no steps and negative steps change nothing", history.todaySteps(), 8);
        history.addSteps(7);
    }
    {
        Database database(file);
        expectEqual("closing saves what was left unsaved", storedSteps(database, "2026-03-10"), 15);
        expectEqual("reopening keeps the schema version", schemaVersion(database), 8);

        time.now = time.now.addSecs(3600);
        StepHistory history(&database, time.clock());
        expectEqual("a restart the same day carries on", history.todaySteps(), 15);
        history.addSteps(10);
        expectEqual("and adds to it", history.todaySteps(), 25);
    }
    Database database(file);
    expectEqual("the restarted count is saved too", storedSteps(database, "2026-03-10"), 25);
}

void testMidnight(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("midnight.db")));
    TestClock time;
    time.now = QDateTime(QDate(2026, 3, 10), QTime(23, 59, 50));
    StepHistory history(&database, time.clock());
    history.addSteps(100);

    time.now = time.now.addSecs(20);
    history.checkDate();
    expectEqual("at midnight the old day is saved", storedSteps(database, "2026-03-10"), 100);
    expectEqual("and the new one starts at 0", history.todaySteps(), 0);
    expect("yesterday moves into the previous days",
           recentDay(history, 0) == QLatin1String("2026-03-10") && recentSteps(history, 0) == 100);

    // No checkDate() this time: the first step of the day finds out by itself.
    time.now = QDateTime(QDate(2026, 3, 12), QTime(8, 0));
    history.addSteps(9);
    expectEqual("a step on a new day counts for the new day", history.todaySteps(), 9);
    expectEqual("and the day before is kept as it was", storedSteps(database, "2026-03-11"), -1);
    expect("a day without steps reads 0 among the previous days",
           recentDay(history, 0) == QLatin1String("2026-03-11") && recentSteps(history, 0) == 0
           && recentDay(history, 1) == QLatin1String("2026-03-10") && recentSteps(history, 1) == 100);

    // The clock is set back to a day already counted.
    time.now = QDateTime(QDate(2026, 3, 10), QTime(18, 0));
    history.checkDate();
    expectEqual("setting the clock back saves the day left", storedSteps(database, "2026-03-12"), 9);
    expectEqual("and carries on with the stored count", history.todaySteps(), 100);
}

void testRecentDays(const QString &directory)
{
    Database database(QDir(directory).filePath(QStringLiteral("recent.db")));
    store(database, QStringLiteral("2026-03-10"), 500);   // today
    store(database, QStringLiteral("2026-03-09"), 4000);
    store(database, QStringLiteral("2026-03-07"), 7000);
    store(database, QStringLiteral("2026-03-06"), 8000);  // four days ago
    store(database, QStringLiteral("2026-03-11"), 9000);  // tomorrow

    TestClock time;
    StepHistory history(&database, time.clock());
    expectEqual("today's stored steps are where the day starts", history.todaySteps(), 500);
    expectEqual("yesterday", recentSteps(history, 0), 4000);
    expectEqual("two days ago, nothing stored", recentSteps(history, 1), 0);
    expectEqual("three days ago", recentSteps(history, 2), 7000);
    expectEqual("four days ago and tomorrow are left out", history.recentDays().size(), 3);
}

void testNewerSchema(const QString &directory)
{
    const QString file = QDir(directory).filePath(QStringLiteral("newer.db"));
    {
        Database database(file);
        QSqlQuery query(database.connection());
        query.exec(QStringLiteral("PRAGMA user_version = 9"));
        store(database, QStringLiteral("2026-03-10"), 42);
    }
    Database database(file);
    expect("a database from a newer version still opens", database.isOpen());
    expectEqual("and is left at its version", schemaVersion(database), 9);
    TestClock time;
    StepHistory history(&database, time.clock());
    expectEqual("and its steps are read", history.todaySteps(), 42);
}

void testNoDatabase(const QString &directory)
{
    // A directory where the file should be: SQLite cannot open it.
    const QString file = QDir(directory).filePath(QStringLiteral("blocked.db"));
    QDir().mkpath(file);
    Database database(file);
    expect("a database that cannot be opened stays closed", !database.isOpen());

    TestClock time;
    StepHistory history(&database, time.clock());
    history.addSteps(12);
    history.save();
    expectEqual("without a database steps are still counted", history.todaySteps(), 12);
    expectEqual("and there are still three previous days", history.recentDays().size(), 3);
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

    testFreshDatabase(directory.path());
    testSaving(directory.path());
    testMidnight(directory.path());
    testRecentDays(directory.path());
    testNewerSchema(directory.path());
    testNoDatabase(directory.path());

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
