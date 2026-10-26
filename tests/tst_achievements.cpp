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
 * reached is kept. Every activity here is generated. Build and run from the
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
                          || unit == "duration" || unit == "count" || unit == "weeks" || unit == "steps");
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

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
