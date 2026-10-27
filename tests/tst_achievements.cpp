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
 * Host-side checks for AchievementCatalogue: the catalogue itself, the level
 * of a value, each metric with the activities and days that count for it and
 * those that do not, the day a level was reached, and that a level once
 * reached is kept. For the calendar categories also their occasions (dates
 * that exist, none twice, Easter by the computus), the occasions of a day,
 * collecting them and keeping what was collected. Every activity here is
 * generated. Build and run from the
 * project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_achievements.cpp \
 *       src/AchievementCatalogue.cpp -o /tmp/tst_achievements && /tmp/tst_achievements
 */

#include "AchievementCatalogue.h"
#include "LandCover.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace AchievementCatalogue;

namespace {

/* 2026-10-12, a Monday, as a Julian day number. */
const int kMonday = 2461326;

/* A Gregorian date's Julian day number. */
int julianDay(int year, int month, int day)
{
    const int a = (14 - month) / 12;
    const int y = year + 4800 - a;
    const int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

int daysIn(int year, int month)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : days[month - 1];
}

/* Easter Sunday as yyyymmdd: the Gregorian computus (Meeus), or the Julian
 * one moved onto the Gregorian calendar, which holds from 1900 to 2099. */
int easter(int year, bool orthodox)
{
    int month;
    int day;
    if (orthodox) {
        const int d = (19 * (year % 19) + 15) % 30;
        const int e = (2 * (year % 4) + 4 * (year % 7) - d + 34) % 7;
        month = (d + e + 114) / 31;
        day = (d + e + 114) % 31 + 1 + 13;
        if (day > daysIn(year, month)) {
            day -= daysIn(year, month);
            ++month;
        }
    } else {
        const int a = year % 19, b = year / 100, c = year % 100, d = b / 4, e = b % 4;
        const int f = (b + 8) / 25, g = (b - f + 1) / 3, h = (19 * a + b - d - g + 15) % 30;
        const int i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7, m = (a + 11 * h + 22 * l) / 451;
        month = (h + l - 7 * m + 114) / 31;
        day = (h + l - 7 * m + 114) % 31 + 1;
    }
    return year * 10000 + month * 100 + day;
}

/* yyyymmdd moved by some days. */
int datePlus(int date, int days)
{
    int day = julianDay(date / 10000, date / 100 % 100, date % 100) + days;
    for (int year = date / 10000 - 1; ; ++year) {
        for (int month = 1; month <= 12; ++month) {
            const int first = julianDay(year, month, 1);
            if (day >= first && day < first + daysIn(year, month))
                return year * 10000 + month * 100 + day - first + 1;
        }
    }
}

int g_failures = 0;

void expect(const char *name, bool ok)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        ++g_failures;
}

void expectEqual(const char *name, double actual, double expected, double tolerance = 1e-9)
{
    const bool ok = std::fabs(actual - expected) <= tolerance;
    std::printf("%s %-58s %12.3f (expected %.3f)\n", ok ? "PASS" : "FAIL", name, actual, expected);
    if (!ok)
        ++g_failures;
}

std::size_t indexOf(const char *key)
{
    for (std::size_t i = 0; i < categoryCount(); ++i) {
        if (std::strcmp(category(i).key, key) == 0)
            return i;
    }
    std::printf("FAIL no category %s\n", key);
    ++g_failures;
    return 0;
}

const Category &categoryOf(const char *key)
{
    return category(indexOf(key));
}

ActivityRecord activity(const char *type, int day, double distance, double duration,
                        double climb = 0)
{
    ActivityRecord record;
    record.type = type;
    record.day = day;
    record.distance = distance;
    record.duration = duration;
    record.climb = climb;
    return record;
}

DayRecord steps(int day, double count)
{
    DayRecord record;
    record.day = day;
    record.steps = count;
    return record;
}

Progress progressOf(const std::vector<Progress> &progress, const char *key)
{
    return progress[indexOf(key)];
}

void testCatalogue()
{
    std::set<std::string> keys;
    bool rising = true;
    bool known = true;
    bool grouped = true;
    std::set<std::string> groupsDone;
    std::string group;
    for (std::size_t i = 0; i < categoryCount(); ++i) {
        const Category &c = category(i);
        keys.insert(c.key);
        for (int level = 1; level < kLevels; ++level)
            rising = rising && c.thresholds[level] > c.thresholds[level - 1];
        rising = rising && c.thresholds[0] > 0;
        const std::string unit = c.unit;
        known = known && (unit == "distance" || unit == "pace" || unit == "speed" || unit == "climb"
                          || unit == "duration" || unit == "count" || unit == "weeks" || unit == "steps"
                          || unit == "days");
        const std::string badge = c.badge;
        known = known && (badge.empty() || badge == "total" || badge == "fastest");
        if (c.group != group) {
            grouped = grouped && groupsDone.insert(c.group).second;
            group = c.group;
        }
        if (c.metric == FastestSpeed)
            known = known && c.minimumDistance > 0;
        if (c.metric == LandCoverDistance)
            known = known && c.landCover > LandCover::Unknown && c.landCover < LandCover::KindCount;
        if (c.metric == CalendarDays)
            known = known && c.occasions && c.thresholds[0] == 1 && c.thresholds[kLevels - 1] == c.occasionCount
                    && unit == "days";
        else
            known = known && !c.occasions && c.occasionCount == 0;
    }
    expect("every category has a key of its own", keys.size() == categoryCount());
    expect("every threshold rises above the one before", rising);
    expect("every unit, badge, minimum distance and land cover is one the page knows", known);
    expect("a group's categories come together", grouped);
    expect("the page has something to show", categoryCount() >= 20);
}

void testLevels()
{
    const Category &longest = categoryOf("running-longest");
    expectEqual("nothing is no level", levelOf(longest, 0), 0);
    expectEqual("just short of the first is none either", levelOf(longest, 999.9), 0);
    expectEqual("the first threshold is the first level", levelOf(longest, 1000), 1);
    expectEqual("between two the lower", levelOf(longest, 7000), 3);
    expectEqual("the last is the last level", levelOf(longest, 100000), kLevels);
    expectEqual("and beyond stays there", levelOf(longest, 1e9), kLevels);
}

void testSingleActivity()
{
    std::vector<ActivityRecord> activities;
    activities.push_back(activity("running", kMonday, 3200, 1200));
    activities.push_back(activity("running", kMonday + 3, 10500, 3600));
    activities.push_back(activity("running", kMonday + 5, 8000, 2700));
    // Further, but not a run.
    activities.push_back(activity("walking", kMonday + 6, 30000, 25000));
    const std::vector<Progress> progress = evaluate(activities, std::vector<DayRecord>());

    const Progress longest = progressOf(progress, "running-longest");
    expectEqual("the longest run is the best of them", longest.value, 10500);
    expectEqual("its level", longest.level, 4);
    expectEqual("dated by the run that reached it", longest.day, kMonday + 3);
    expectEqual("a walk is not a run", progressOf(progress, "walking-longest").value, 30000);
    expectEqual("nor counts for cycling", progressOf(progress, "cycling-longest").value, 0);
    expectEqual("which has no level", progressOf(progress, "cycling-longest").level, 0);
    expectEqual("the longest activity by time, whatever type", progressOf(progress, "duration-single").value,
                25000);
}

void testTotals()
{
    std::vector<ActivityRecord> activities;
    for (int i = 0; i < 10; ++i)
        activities.push_back(activity("cycling", kMonday + i, 12000, 2400, 60));
    activities.push_back(activity("driving", kMonday + 20, 300000, 14400, 900));
    const std::vector<Progress> progress = evaluate(activities, std::vector<DayRecord>());

    const Progress distance = progressOf(progress, "cycling-distance");
    expectEqual("the distances add up", distance.value, 120000);
    expectEqual("to a level", distance.level, 2);
    // 25 km on the third ride, 100 km on the ninth.
    expectEqual("reached with the ride that crossed it", distance.day, kMonday + 8);
    expectEqual("the climbs add up", progressOf(progress, "climb-total").value, 600);
    expectEqual("the biggest climb is one ride's", progressOf(progress, "climb-single").value, 60);
    expectEqual("the time adds up", progressOf(progress, "duration-total").value, 24000);
    expectEqual("and so do the activities", progressOf(progress, "activities-count").value, 10);
    expect("a drive counts for none of them",
           progressOf(progress, "climb-single").value < 900 && progressOf(progress, "duration-single").value < 14400);
}

void testFastest()
{
    const Category &pace = categoryOf("running-pace");
    std::vector<ActivityRecord> activities;
    // 4:00 per km, but too short to count.
    activities.push_back(activity("running", kMonday, pace.minimumDistance - 10, 0.24 * (pace.minimumDistance - 10)));
    // 6:00 per km.
    activities.push_back(activity("running", kMonday + 1, 6000, 2160));
    // 5:00 per km.
    activities.push_back(activity("running", kMonday + 2, 5000, 1500));
    // No time at all, as an activity of a single point.
    activities.push_back(activity("running", kMonday + 3, 7000, 0));
    const std::vector<Progress> progress = evaluate(activities, std::vector<DayRecord>());

    const Progress fastest = progressOf(progress, "running-pace");
    expectEqual("the fastest run long enough", fastest.value, 5000.0 / 1500);
    expectEqual("comes to a level", fastest.level, 4);
    expectEqual("on its day", fastest.day, kMonday + 2);
}

void testLandCover()
{
    std::vector<ActivityRecord> activities;
    ActivityRecord walk = activity("walking", kMonday, 6000, 4000);
    walk.landCover.assign(LandCover::KindCount, 0.0);
    walk.landCover[LandCover::Forest] = 4000;
    walk.landCover[LandCover::Water] = 500;
    walk.landCover[LandCover::Unknown] = 1500;
    activities.push_back(walk);
    // Not known yet.
    activities.push_back(activity("running", kMonday + 1, 3000, 1000));
    ActivityRecord drive = activity("driving", kMonday + 2, 40000, 2000);
    drive.landCover.assign(LandCover::KindCount, 0.0);
    drive.landCover[LandCover::Forest] = 40000;
    activities.push_back(drive);
    walk.day = kMonday + 3;
    activities.push_back(walk);
    const std::vector<Progress> progress = evaluate(activities, std::vector<DayRecord>());

    const Progress forest = progressOf(progress, "terrain-forest");
    expectEqual("the forest adds up over the activities", forest.value, 8000);
    expectEqual("to a level", forest.level, 2);
    expectEqual("reached the second time", forest.day, kMonday + 3);
    expectEqual("water on its own", progressOf(progress, "terrain-water").value, 1000);
    expectEqual("land without a tile counts for nothing", progressOf(progress, "terrain-builtup").value, 0);
}

void testStreak()
{
    std::vector<ActivityRecord> activities;
    // Weeks 0, 1 (twice) and 2; a week left out; weeks 4 to 7; a Sunday and
    // the Monday after it are two weeks.
    const int days[] = { 2, 7, 11, 16, 30, 35, 42, 49, 55, 56 };
    for (int day : days)
        activities.push_back(activity("walking", kMonday + day, 1000, 600));
    activities.push_back(activity("driving", kMonday + 70, 1000, 600));
    const std::vector<Progress> progress = evaluate(activities, std::vector<DayRecord>());

    const Progress streak = progressOf(progress, "streak-weeks");
    expectEqual("the longest run of weeks with an activity", streak.value, 5);
    expectEqual("comes to a level", streak.level, 2);
    expectEqual("reached in the fourth week of the second run", streak.day, kMonday + 49);

    std::vector<ActivityRecord> once;
    once.push_back(activity("rowing", kMonday + 3, 1000, 600));
    expectEqual("a single activity is a week", progressOf(evaluate(once, std::vector<DayRecord>()), "streak-weeks").value,
                1);
    once.insert(once.begin(), activity("rowing", kMonday - 1, 1000, 600));
    expectEqual("the Sunday before makes two", progressOf(evaluate(once, std::vector<DayRecord>()), "streak-weeks").value,
                2);

    std::vector<ActivityRecord> backwards;
    backwards.push_back(activity("running", kMonday + 14, 1000, 600));
    // The clock set back.
    backwards.push_back(activity("running", kMonday, 1000, 600));
    backwards.push_back(activity("running", kMonday + 21, 1000, 600));
    expectEqual("an activity out of order breaks nothing",
                progressOf(evaluate(backwards, std::vector<DayRecord>()), "streak-weeks").value, 2);
}

void testSteps()
{
    std::vector<DayRecord> days;
    days.push_back(steps(kMonday, 4000));
    days.push_back(steps(kMonday + 1, 12000));
    days.push_back(steps(kMonday + 2, 9000));
    days.push_back(steps(kMonday + 3, 0));
    const std::vector<Progress> progress = evaluate(std::vector<ActivityRecord>(), days);

    const Progress best = progressOf(progress, "steps-day");
    expectEqual("the most steps in a day", best.value, 12000);
    expectEqual("its level", best.level, 4);
    expectEqual("on that day", best.day, kMonday + 1);
    const Progress total = progressOf(progress, "steps-total");
    expectEqual("all days' steps together", total.value, 25000);
    expectEqual("reach their level", total.level, 1);
    expectEqual("on the day they did", total.day, kMonday + 1);
    expectEqual("steps are no activity", progressOf(progress, "activities-count").value, 0);
}

void testNothing()
{
    const std::vector<Progress> progress = evaluate(std::vector<ActivityRecord>(), std::vector<DayRecord>());
    bool none = progress.size() == categoryCount();
    for (const Progress &p : progress)
        none = none && p.level == 0 && p.value == 0;
    expect("without activities or steps every category is at nothing", none);
}

void testKeeping()
{
    Award kept;
    Progress progress;
    progress.value = 12000;
    progress.level = 4;
    progress.day = kMonday;
    Award award = merge(kept, progress);
    expect("a level reached is kept", award.level == 4 && award.day == kMonday && award.best == 12000);

    // The run that reached it deleted.
    progress.value = 4000;
    progress.level = 2;
    progress.day = kMonday - 10;
    award = merge(award, progress);
    expect("and stays when the activities say less", award.level == 4 && award.day == kMonday);
    expectEqual("as does the best value", award.best, 12000);

    progress.value = 11000;
    progress.level = 4;
    progress.day = kMonday + 5;
    award = merge(award, progress);
    expectEqual("the same level again keeps its first day", award.day, kMonday);

    progress.value = 22000;
    progress.level = 5;
    progress.day = kMonday + 9;
    award = merge(award, progress);
    expect("a higher one takes over, dated anew", award.level == 5 && award.day == kMonday + 9 && award.best == 22000);
}


/* The index of an occasion in its category, or the count if not there. */
std::size_t occasionOf(const Category &c, const char *key)
{
    for (std::size_t i = 0; i < c.occasionCount; ++i) {
        if (std::strcmp(c.occasions[i].key, key) == 0)
            return i;
    }
    std::printf("FAIL no occasion %s in %s\n", key, c.key);
    ++g_failures;
    return c.occasionCount;
}

bool collectedHas(const Progress &progress, std::size_t occasion, int day)
{
    for (const Collected &c : progress.collected) {
        if (c.occasion == occasion)
            return c.day == day;
    }
    return false;
}

void testOccasions()
{
    std::set<std::string> keys;
    std::size_t total = 0;
    bool exist = true;
    bool once = true;
    bool listed = true;
    bool ordered = true;
    for (std::size_t i = 0; i < categoryCount(); ++i) {
        const Category &c = category(i);
        std::set<int> fixed;
        int previous = 0;
        for (std::size_t j = 0; j < c.occasionCount; ++j) {
            const Occasion &o = c.occasions[j];
            keys.insert(o.key);
            ++total;
            if (o.month > 0) {
                exist = exist && o.month <= 12 && o.day >= 1 && o.day <= daysIn(2000, o.month)
                        && !(o.month == 2 && o.day == 29) && o.dates[0] == 0;
                once = once && fixed.insert(o.month * 100 + o.day).second;
                ordered = ordered && o.month * 100 + o.day > previous;
                previous = o.month * 100 + o.day;
                continue;
            }
            std::set<int> years;
            int last = 0;
            for (int k = 0; k < kOccasionDates && o.dates[k] != 0; ++k) {
                const int date = o.dates[k];
                const int month = date / 100 % 100;
                exist = exist && month >= 1 && month <= 12 && date % 100 >= 1
                        && date % 100 <= daysIn(date / 10000, month) && date > last;
                last = date;
                years.insert(date / 10000);
            }
            for (int year = 2026; year <= 2031; ++year)
                listed = listed && years.count(year) == 1;
            listed = listed && o.year == 0;
        }
    }
    expect("every occasion has a key of its own", keys.size() == total);
    expect("every date of every occasion exists, none on the 29th of February", exist);
    expect("no date of every year twice in a category", once);
    expect("and those in the order of the year", ordered);
    expect("every feast that moves is listed for each year from 2026 to 2031", listed);

    const Category &worshipper = categoryOf("days-worshipper");
    const struct { const char *key; int offset; bool orthodox; } easterDays[] = {
        { "ash-wednesday", -46, false }, { "good-friday", -2, false }, { "easter", 0, false },
        { "orthodox-easter", 0, true }, { "ascension", 39, false }, { "pentecost", 49, false },
        { "corpus-christi", 60, false },
    };
    bool computus = true;
    for (const auto &feast : easterDays) {
        const Occasion &o = worshipper.occasions[occasionOf(worshipper, feast.key)];
        for (int k = 0; k < 6; ++k)
            computus = computus && o.dates[k] == datePlus(easter(2026 + k, feast.orthodox), feast.offset);
        computus = computus && o.dates[6] == 0;
    }
    expect("Easter and the feasts it moves follow the computus", computus);
    const Occasion &ramadan = worshipper.occasions[occasionOf(worshipper, "ramadan")];
    expect("a lunar year's feast comes twice in some solar years", ramadan.dates[6] != 0);
}

void testOccasionsOn()
{
    expectEqual("the test's Julian days are SQLite's and QDate's", julianDay(2026, 10, 12), kMonday);
    const Category &first = categoryOf("days-first-one");
    const std::vector<std::size_t> moon = occasionsOn(first, julianDay(2027, 7, 20));
    expect("a date of every year, in any year", moon.size() == 1 && moon[0] == occasionOf(first, "moon-landing"));
    expect("the day before is none", occasionsOn(first, julianDay(2027, 7, 19)).empty());
    expect("nor is it another category's", occasionsOn(categoryOf("days-virtuoso"), julianDay(2027, 7, 20)).empty());
    expect("an occasion of the first of January", occasionsOn(categoryOf("days-explorer"), julianDay(2030, 1, 1)).size() == 1);
    expect("and of the 31st of December", occasionsOn(categoryOf("days-freedom-fighter"),
                                                      julianDay(2029, 12, 30)).size() == 1);

    const Category &worshipper = categoryOf("days-worshipper");
    const std::vector<std::size_t> easter2028 = occasionsOn(worshipper, julianDay(2028, 4, 16));
    expect("two feasts that move onto one day", easter2028.size() == 2
           && easter2028[0] == occasionOf(worshipper, "easter")
           && easter2028[1] == occasionOf(worshipper, "orthodox-easter"));
    expect("a feast that moves only on its dates", occasionsOn(worshipper, julianDay(2027, 4, 16)).empty());
    expect("not after the years listed", occasionsOn(worshipper, julianDay(2032, 3, 28)).empty());
    expect("while one of every year still is",
           occasionsOn(worshipper, julianDay(2032, 12, 25)).size() == 1);
    // Hanukkah begins on Christmas Day 2027.
    expect("one that moves onto one of every year", occasionsOn(worshipper, julianDay(2027, 12, 25)).size() == 2);
}

void testCalendar()
{
    std::vector<ActivityRecord> activities;
    activities.push_back(activity("walking", julianDay(2026, 12, 25), 3000, 1800));
    // The same occasions a year later.
    activities.push_back(activity("running", julianDay(2027, 12, 25), 5000, 1800));
    activities.push_back(activity("driving", julianDay(2027, 7, 4), 50000, 3600));
    activities.push_back(activity("cycling", julianDay(2028, 4, 16), 20000, 3600));
    // The clock set back: an earlier day, out of order.
    activities.push_back(activity("rowing", julianDay(2026, 12, 6), 2000, 1200));
    activities.push_back(activity("rowing", julianDay(2026, 7, 4), 2000, 1200));
    const std::vector<Progress> progress = evaluate(activities, std::vector<DayRecord>());

    const Category &worshipper = categoryOf("days-worshipper");
    const Progress feasts = progressOf(progress, "days-worshipper");
    // Christmas 2026, Christmas and Hanukkah 2027, Easter and Orthodox Easter 2028.
    expectEqual("each occasion counts once", feasts.value, 4);
    expectEqual("to a level", feasts.level, 2);
    expectEqual("reached by the third occasion", feasts.day, julianDay(2028, 4, 16));
    expect("dated by the first activity on it",
           collectedHas(feasts, occasionOf(worshipper, "christmas"), julianDay(2026, 12, 25)));
    expect("and collected in order of occasion", feasts.collected.size() == 4
           && feasts.collected[0].occasion < feasts.collected[3].occasion);

    const Category &patriot = categoryOf("days-patriot");
    const Progress patriotic = progressOf(progress, "days-patriot");
    expectEqual("a drive collects nothing", patriotic.value, 2);
    expect("an activity out of order is the earliest of its occasion",
           collectedHas(patriotic, occasionOf(patriot, "united-states-independence-day"), julianDay(2026, 7, 4))
           && collectedHas(patriotic, occasionOf(patriot, "finland-independence-day"), julianDay(2026, 12, 6)));
    expectEqual("the first occasion is the first level", patriotic.level, 1);
    expectEqual("on the earliest day", patriotic.day, julianDay(2026, 7, 4));
    expectEqual("a category without any", progressOf(progress, "days-virtuoso").level, 0);

    // A walk every day of 2027 collects every occasion there is.
    std::vector<ActivityRecord> year;
    for (int day = julianDay(2027, 1, 1); day <= julianDay(2027, 12, 31); ++day)
        year.push_back(activity("walking", day, 2000, 1200));
    const std::vector<Progress> all = evaluate(year, std::vector<DayRecord>());
    bool every = true;
    for (std::size_t i = 0; i < categoryCount(); ++i) {
        if (category(i).metric == CalendarDays)
            every = every && all[i].value == category(i).occasionCount && all[i].level == kLevels;
    }
    expect("every day of a year is every level of every category", every);
    expectEqual("the last national day is Kenya's", progressOf(all, "days-patriot").day, julianDay(2027, 12, 12));
    expectEqual("the last feast of 2027 Christmas, with Hanukkah", progressOf(all, "days-worshipper").day,
                julianDay(2027, 12, 25));
}

void testCollecting()
{
    const Category &explorer = categoryOf("days-explorer");
    const std::size_t pole = occasionOf(explorer, "south-pole");
    const std::size_t americas = occasionOf(explorer, "columbus-americas");
    std::vector<ActivityRecord> activities;
    activities.push_back(activity("walking", julianDay(2027, 12, 14), 3000, 1800));
    Progress now = progressOf(evaluate(activities, std::vector<DayRecord>()), "days-explorer");

    std::vector<Collected> kept;
    Collected columbus;
    columbus.occasion = americas;
    columbus.day = julianDay(2026, 10, 12);
    kept.push_back(columbus);
    // Collected later than the activity now says: the activity's day wins.
    Collected later;
    later.occasion = pole;
    later.day = julianDay(2028, 12, 14);
    kept.push_back(later);
    // A key gone from the catalogue would not be read; an index past the
    // end is left out all the same.
    Collected gone;
    gone.occasion = explorer.occasionCount + 3;
    gone.day = julianDay(2026, 1, 1);
    kept.push_back(gone);

    const Progress merged = collect(explorer, now, kept);
    expectEqual("what was collected before counts with what the activities collect", merged.value, 2);
    expect("each from the earliest day", collectedHas(merged, americas, julianDay(2026, 10, 12))
           && collectedHas(merged, pole, julianDay(2027, 12, 14)));
    expectEqual("the level from the occasions", merged.level, 1);
    expectEqual("dated by the first", merged.day, julianDay(2026, 10, 12));
    expectEqual("nothing collected keeps nothing", collect(explorer, Progress(), std::vector<Collected>()).value, 0);

    // The activities deleted: the occasions stay.
    const Progress deleted = collect(explorer, progressOf(evaluate(std::vector<ActivityRecord>(),
                                                                   std::vector<DayRecord>()), "days-explorer"),
                                     merged.collected);
    expect("what was collected stays when its activities are deleted",
           deleted.value == 2 && deleted.level == 1 && deleted.collected.size() == 2);
}

} // namespace

int main()
{
    testCatalogue();
    testLevels();
    testSingleActivity();
    testTotals();
    testFastest();
    testLandCover();
    testStreak();
    testSteps();
    testNothing();
    testKeeping();
    testOccasions();
    testOccasionsOn();
    testCalendar();
    testCollecting();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
